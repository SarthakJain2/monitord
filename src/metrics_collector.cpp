#include "metrics_collector.hpp"
#include <sstream>
#include <iomanip>
#include <sys/sysctl.h>
#include <mach/vm_statistics.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <sys/statvfs.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <cmath>

// CPU state constants for macOS
#ifndef CPUSTATES
#define CPUSTATES 4
#define CP_USER 0
#define CP_NICE 1
#define CP_SYS 2
#define CP_IDLE 3
#endif

namespace http
{

    std::string SystemMetrics::ToJson() const
    {
        std::ostringstream json;
        json << std::fixed << std::setprecision(2);

        auto time_t = std::chrono::system_clock::to_time_t(timestamp);

        json << "{"
             << "\"timestamp\":" << time_t << "000,"
             << "\"cpu\":{"
             << "\"percent\":" << cpu_percent << ","
             << "\"user\":" << cpu_user << ","
             << "\"system\":" << cpu_system << ","
             << "\"idle\":" << cpu_idle
             << "},"
             << "\"memory\":{"
             << "\"total\":" << memory_total << ","
             << "\"used\":" << memory_used << ","
             << "\"free\":" << memory_free << ","
             << "\"percent\":" << memory_percent
             << "},"
             << "\"disk\":{"
             << "\"total\":" << disk_total << ","
             << "\"used\":" << disk_used << ","
             << "\"free\":" << disk_free << ","
             << "\"percent\":" << disk_percent
             << "},"
             << "\"disk_io\":{"
             << "\"reads\":" << disk_reads << ","
             << "\"writes\":" << disk_writes << ","
             << "\"data_read\":" << disk_data_read << ","
             << "\"data_written\":" << disk_data_written << ","
             << "\"read_rate\":" << disk_read_rate << ","
             << "\"write_rate\":" << disk_write_rate << ","
             << "\"data_read_rate\":" << disk_data_read_rate << ","
             << "\"data_write_rate\":" << disk_data_write_rate
             << "},"
             << "\"network\":{"
             << "\"rx_bytes\":" << network_rx_bytes << ","
             << "\"tx_bytes\":" << network_tx_bytes << ","
             << "\"rx_packets\":" << network_rx_packets << ","
             << "\"tx_packets\":" << network_tx_packets << ","
             << "\"rx_rate\":" << network_rx_rate << ","
             << "\"tx_rate\":" << network_tx_rate
             << "}"
             << "}";

        return json.str();
    }

    MetricsCollector::MetricsCollector()
        : prev_user_time_(0), prev_system_time_(0), prev_idle_time_(0),
          prev_cpu_time_(std::chrono::system_clock::now()),
          first_collection_(true),
          prev_rx_bytes_(0), prev_tx_bytes_(0),
          prev_rx_packets_(0), prev_tx_packets_(0),
          prev_network_time_(std::chrono::system_clock::now()),
          first_network_collection_(true),
          prev_disk_reads_(0), prev_disk_writes_(0),
          prev_disk_data_read_(0), prev_disk_data_written_(0),
          prev_disk_io_time_(std::chrono::system_clock::now()),
          first_disk_io_collection_(true)
    {
    }

    SystemMetrics MetricsCollector::Collect()
    {
        SystemMetrics metrics;

        // Collect CPU
        metrics.cpu_percent = GetCpuUsage();

        // Get detailed CPU breakdown for display
        uint64_t user, system, idle, total;
        ReadCpuTimes(user, system, idle, total);

        if (!first_collection_ && total > 0)
        {
            // Calculate percentages based on current snapshot
            // These are cumulative, so we need to use the differences
            uint64_t user_diff = (user >= prev_user_time_) ? (user - prev_user_time_) : 0;
            uint64_t system_diff = (system >= prev_system_time_) ? (system - prev_system_time_) : 0;
            uint64_t idle_diff = (idle >= prev_idle_time_) ? (idle - prev_idle_time_) : 0;
            uint64_t total_diff = user_diff + system_diff + idle_diff;

            if (total_diff > 0)
            {
                metrics.cpu_user = 100.0 * static_cast<double>(user_diff) / total_diff;
                metrics.cpu_system = 100.0 * static_cast<double>(system_diff) / total_diff;
                metrics.cpu_idle = 100.0 * static_cast<double>(idle_diff) / total_diff;
            }
        }

        // Collect memory
        GetMemoryUsage(metrics.memory_total, metrics.memory_used, metrics.memory_free);
        if (metrics.memory_total > 0)
        {
            metrics.memory_percent = (static_cast<double>(metrics.memory_used) / metrics.memory_total) * 100.0;
        }

        // Collect disk
        GetDiskUsage(metrics.disk_total, metrics.disk_used, metrics.disk_free);
        {
            // Match macOS `df`/Activity Monitor "Capacity":
            // Capacity = Used / (Used + Avail), not Used / Total (which includes reserved blocks).
            const double used_plus_free =
                static_cast<double>(metrics.disk_used) + static_cast<double>(metrics.disk_free);
            if (used_plus_free > 0.0)
            {
                metrics.disk_percent =
                    (static_cast<double>(metrics.disk_used) / used_plus_free) * 100.0;
            }
            else
            {
                metrics.disk_percent = 0.0;
            }
        }

        // Collect disk I/O
        GetDiskIOStats(metrics.disk_reads, metrics.disk_writes,
                       metrics.disk_data_read, metrics.disk_data_written,
                       metrics.disk_read_rate, metrics.disk_write_rate,
                       metrics.disk_data_read_rate, metrics.disk_data_write_rate);

        // Collect network
        GetNetworkStats(metrics.network_rx_bytes, metrics.network_tx_bytes,
                        metrics.network_rx_packets, metrics.network_tx_packets,
                        metrics.network_rx_rate, metrics.network_tx_rate);

        metrics.timestamp = std::chrono::system_clock::now();

        return metrics;
    }

    double MetricsCollector::GetCpuUsage()
    {
        uint64_t user, system, idle, total;
        ReadCpuTimes(user, system, idle, total);

        if (first_collection_)
        {
            prev_user_time_ = user;
            prev_system_time_ = system;
            prev_idle_time_ = idle;
            prev_cpu_time_ = std::chrono::system_clock::now();
            first_collection_ = false;
            // Return 0 for first collection (need a baseline)
            return 0.0;
        }

        auto now = std::chrono::system_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev_cpu_time_).count();

        // Need at least 200ms between samples for accurate calculation
        // (We collect every 1 second, so this should always be true)
        if (time_diff < 200)
        {
            return 0.0;
        }

        // Calculate differences (handle wraparound - though unlikely with uint64_t)
        uint64_t user_diff = (user >= prev_user_time_) ? (user - prev_user_time_) : user;
        uint64_t system_diff = (system >= prev_system_time_) ? (system - prev_system_time_) : system;
        uint64_t idle_diff = (idle >= prev_idle_time_) ? (idle - prev_idle_time_) : idle;

        uint64_t total_diff = user_diff + system_diff + idle_diff;

        if (total_diff == 0)
        {
            // No change, return previous value or 0
            return 0.0;
        }

        // CPU usage = (user + system) / total
        // This gives us the percentage of time spent in non-idle states
        double cpu_usage = 100.0 * static_cast<double>(user_diff + system_diff) / total_diff;

        // Update previous values
        prev_user_time_ = user;
        prev_system_time_ = system;
        prev_idle_time_ = idle;
        prev_cpu_time_ = now;

        return std::max(0.0, std::min(100.0, cpu_usage));
    }

    void MetricsCollector::ReadCpuTimes(uint64_t &user, uint64_t &system, uint64_t &idle, uint64_t &total)
    {
        // Use host_processor_info to get CPU load info (more reliable on macOS)
        processor_info_array_t cpu_info;
        mach_msg_type_number_t num_cpu_info;
        natural_t num_processors;

        kern_return_t result = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                                   &num_processors, &cpu_info, &num_cpu_info);

        if (result != KERN_SUCCESS)
        {
            user = system = idle = total = 0;
            return;
        }

        // Sum up all CPU states across all processors
        user = system = idle = 0;

        processor_cpu_load_info_t cpu_load_info = (processor_cpu_load_info_t)cpu_info;
        for (natural_t i = 0; i < num_processors; i++)
        {
            user += cpu_load_info[i].cpu_ticks[CPU_STATE_USER];
            system += cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM];
            idle += cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE];
        }

        total = user + system + idle;

        // Free the memory allocated by host_processor_info
        vm_deallocate(mach_task_self(), (vm_address_t)cpu_info, num_cpu_info * sizeof(natural_t));
    }

    void MetricsCollector::GetMemoryUsage(uint64_t &total, uint64_t &used, uint64_t &free)
    {
        vm_size_t page_size;
        vm_statistics64_data_t vm_stat;
        mach_port_t mach_port = mach_host_self();
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

        // Get total physical memory
        int64_t memsize;
        size_t memsize_size = sizeof(memsize);
        if (sysctlbyname("hw.memsize", &memsize, &memsize_size, nullptr, 0) != 0)
        {
            total = used = free = 0;
            return;
        }
        total = static_cast<uint64_t>(memsize);

        if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_port, HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) == KERN_SUCCESS)
        {

            uint64_t free_pages = vm_stat.free_count;
            uint64_t inactive_pages = vm_stat.inactive_count;
            uint64_t speculative_pages = vm_stat.speculative_count;
            uint64_t purgeable_pages = vm_stat.purgeable_count;
            uint64_t active_pages = vm_stat.active_count;
            uint64_t wired_pages = vm_stat.wire_count;
            uint64_t compressed_pages = vm_stat.compressor_page_count;

            uint64_t page_size_u64 = static_cast<uint64_t>(page_size);

            // Activity Monitor's "Memory Used" = Active + Wired + Compressed
            // This matches the breakdown shown: App Memory + Wired Memory + Compressed
            // This gives us ~75% usage, which is closer to Activity Monitor's display
            uint64_t used_pages = active_pages + wired_pages + compressed_pages;
            used = used_pages * page_size_u64;

            // Available memory = Free + Inactive + Speculative + Purgeable
            // This approximates "Cached Files" + truly free memory
            uint64_t available_pages = free_pages + inactive_pages + speculative_pages + purgeable_pages;
            free = available_pages * page_size_u64;

            // Safety: ensure values don't exceed total
            if (used > total)
                used = total;
            if (free > total)
                free = total;
        }
        else
        {
            total = used = free = 0;
        }
    }

    void MetricsCollector::GetDiskUsage(uint64_t &total, uint64_t &used, uint64_t &free)
    {
        // Match macOS `df -k /` output so users see the same numbers here as
        // in Activity Monitor / Finder for the root volume.
        FILE *fp = popen("df -k / | tail -1", "r");
        if (fp)
        {
            char line[512];
            if (fgets(line, sizeof(line), fp))
            {
                // Expected format (whitespace-separated):
                // Filesystem  1024-blocks      Used     Avail  Capacity  ...
                // We care about columns: total_kb (2), used_kb (3), avail_kb (4)
                char fs[128];
                unsigned long long total_kb = 0, used_kb = 0, avail_kb = 0;
                int scanned = std::sscanf(line, "%127s %llu %llu %llu", fs, &total_kb, &used_kb, &avail_kb);
                if (scanned == 4)
                {
                    total = total_kb * 1024ULL;
                    used = used_kb * 1024ULL;
                    free = avail_kb * 1024ULL;
                }
                else
                {
                    total = used = free = 0;
                }
            }
            else
            {
                total = used = free = 0;
            }
            pclose(fp);
            return;
        }

        // Fallback: statvfs (may not exactly match df on APFS, but better than nothing)
        struct statvfs stat;
        if (statvfs("/", &stat) == 0)
        {
            total = stat.f_blocks * stat.f_frsize;
            free = stat.f_bavail * stat.f_frsize;
            used = total - free;
        }
        else
        {
            total = used = free = 0;
        }
    }

    void MetricsCollector::GetNetworkStats(uint64_t &rx_bytes, uint64_t &tx_bytes,
                                           uint64_t &rx_packets, uint64_t &tx_packets,
                                           double &rx_rate, double &tx_rate)
    {
        ReadNetworkStats(rx_bytes, tx_bytes, rx_packets, tx_packets);

        auto now = std::chrono::system_clock::now();

        if (first_network_collection_)
        {
            prev_rx_bytes_ = rx_bytes;
            prev_tx_bytes_ = tx_bytes;
            prev_rx_packets_ = rx_packets;
            prev_tx_packets_ = tx_packets;
            prev_network_time_ = now;
            first_network_collection_ = false;
            rx_rate = tx_rate = 0.0;
            return;
        }

        // Calculate rate (bytes per second)
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev_network_time_).count();
        if (time_diff > 0)
        {
            uint64_t rx_diff = (rx_bytes >= prev_rx_bytes_) ? (rx_bytes - prev_rx_bytes_) : 0;
            uint64_t tx_diff = (tx_bytes >= prev_tx_bytes_) ? (tx_bytes - prev_tx_bytes_) : 0;

            rx_rate = (rx_diff * 1000.0) / time_diff; // bytes per second
            tx_rate = (tx_diff * 1000.0) / time_diff; // bytes per second
        }
        else
        {
            rx_rate = tx_rate = 0.0;
        }

        prev_rx_bytes_ = rx_bytes;
        prev_tx_bytes_ = tx_bytes;
        prev_rx_packets_ = rx_packets;
        prev_tx_packets_ = tx_packets;
        prev_network_time_ = now;
    }

    void MetricsCollector::ReadNetworkStats(uint64_t &rx_bytes, uint64_t &tx_bytes,
                                            uint64_t &rx_packets, uint64_t &tx_packets)
    {
        rx_bytes = tx_bytes = rx_packets = tx_packets = 0;

        struct ifaddrs *ifaddrs_ptr = nullptr;
        if (getifaddrs(&ifaddrs_ptr) == 0)
        {
            for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next)
            {
                if (ifa->ifa_addr == nullptr)
                    continue;
                if (ifa->ifa_addr->sa_family != AF_LINK)
                    continue;

                struct if_data *if_data = (struct if_data *)ifa->ifa_data;
                if (if_data == nullptr)
                    continue;

                // Skip loopback
                if (ifa->ifa_flags & IFF_LOOPBACK)
                    continue;

                rx_bytes += if_data->ifi_ibytes;
                tx_bytes += if_data->ifi_obytes;
                rx_packets += if_data->ifi_ipackets;
                tx_packets += if_data->ifi_opackets;
            }
            freeifaddrs(ifaddrs_ptr);
        }
    }

    void MetricsCollector::GetDiskIOStats(uint64_t &reads, uint64_t &writes,
                                          uint64_t &data_read, uint64_t &data_written,
                                          double &read_rate, double &write_rate,
                                          double &data_read_rate, double &data_write_rate)
    {
        ReadDiskIOStats(reads, writes, data_read, data_written);

        auto now = std::chrono::system_clock::now();

        if (first_disk_io_collection_)
        {
            prev_disk_reads_ = reads;
            prev_disk_writes_ = writes;
            prev_disk_data_read_ = data_read;
            prev_disk_data_written_ = data_written;
            prev_disk_io_time_ = now;
            first_disk_io_collection_ = false;
            read_rate = write_rate = data_read_rate = data_write_rate = 0.0;
            return;
        }

        // Calculate rates
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev_disk_io_time_).count();
        if (time_diff > 0)
        {
            uint64_t reads_diff = (reads >= prev_disk_reads_) ? (reads - prev_disk_reads_) : 0;
            uint64_t writes_diff = (writes >= prev_disk_writes_) ? (writes - prev_disk_writes_) : 0;
            uint64_t data_read_diff = (data_read >= prev_disk_data_read_) ? (data_read - prev_disk_data_read_) : 0;
            uint64_t data_written_diff = (data_written >= prev_disk_data_written_) ? (data_written - prev_disk_data_written_) : 0;

            read_rate = (reads_diff * 1000.0) / time_diff;              // ops per second
            write_rate = (writes_diff * 1000.0) / time_diff;            // ops per second
            data_read_rate = (data_read_diff * 1000.0) / time_diff;     // bytes per second
            data_write_rate = (data_written_diff * 1000.0) / time_diff; // bytes per second
        }
        else
        {
            read_rate = write_rate = data_read_rate = data_write_rate = 0.0;
        }

        prev_disk_reads_ = reads;
        prev_disk_writes_ = writes;
        prev_disk_data_read_ = data_read;
        prev_disk_data_written_ = data_written;
        prev_disk_io_time_ = now;
    }

    void MetricsCollector::ReadDiskIOStats(uint64_t &reads, uint64_t &writes,
                                           uint64_t &data_read, uint64_t &data_written)
    {
        // On macOS, iostat -I shows cumulative totals:
        // Format: KB/t xfrs MB
        // where xfrs = total transfers (reads + writes combined)
        // and MB = total megabytes transferred

        // Get cumulative totals from iostat -I
        FILE *fp = popen("iostat -I 1 1 2>/dev/null | tail -1 | awk '{print $2, $3}'", "r");
        if (fp)
        {
            char line[256];
            if (fgets(line, sizeof(line), fp))
            {
                unsigned long long total_xfrs = 0;
                double total_mb = 0.0;

                // Parse: xfrs MB
                if (std::sscanf(line, "%llu %lf", &total_xfrs, &total_mb) == 2)
                {
                    // macOS iostat doesn't separate reads from writes
                    // Estimate: typically reads are 60-70% of total operations
                    // This is a heuristic - Activity Monitor gets exact values from IOKit
                    const double read_ratio = 0.65; // 65% reads, 35% writes (typical)

                    reads = static_cast<uint64_t>(total_xfrs * read_ratio);
                    writes = total_xfrs - reads;

                    // For data, estimate based on typical read/write data ratio
                    // Reads often transfer more data (file access), writes are often smaller
                    const double data_read_ratio = 0.70; // 70% of data is reads
                    uint64_t total_data_bytes = static_cast<uint64_t>(total_mb * 1024.0 * 1024.0);
                    data_read = static_cast<uint64_t>(total_data_bytes * data_read_ratio);
                    data_written = total_data_bytes - data_read;

                    pclose(fp);
                    return;
                }
            }
            pclose(fp);
        }

        // Fallback: return 0 if iostat fails
        reads = writes = data_read = data_written = 0;
    }

} // namespace http
