#include "server.hpp"
#include "http_response.hpp"
#include "metrics_collector.hpp"
#include "metrics_storage.hpp"
#include "alert_manager.hpp"
#include "websocket.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <vector>
#include <sstream>
#include <ctime>
#include <thread>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>

using namespace http;

std::atomic<bool> g_running{true};
Server* g_server = nullptr;
MetricsCollector* g_collector = nullptr;
MetricsStorage* g_storage = nullptr;
AlertManager* g_alert_manager = nullptr;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server...\n";
        g_running = false;
        if (g_server) {
            g_server->Stop();
        }
    }
}

void MetricsCollectionThread() {
    while (g_running) {
        if (g_collector && g_storage && g_alert_manager) {
            SystemMetrics metrics = g_collector->Collect();
            g_storage->AddSample(metrics);
            g_alert_manager->CheckMetrics(metrics);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::string GetDashboardHTML() {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>System Monitoring Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
            padding: 20px;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        h1 {
            color: white;
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .stat-card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            transition: transform 0.2s;
        }
        .stat-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 6px 12px rgba(0,0,0,0.15);
        }
        .stat-label {
            font-size: 0.9em;
            color: #666;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .stat-value {
            font-size: 2.5em;
            font-weight: bold;
            color: #667eea;
        }
        .stat-unit {
            font-size: 0.6em;
            color: #999;
            margin-left: 5px;
        }
        .charts-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .chart-card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .chart-title {
            font-size: 1.3em;
            margin-bottom: 20px;
            color: #333;
            font-weight: 600;
        }
        .alerts-section {
            background: white;
            border-radius: 12px;
            padding: 25px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .alert {
            padding: 15px;
            margin: 10px 0;
            border-radius: 8px;
            border-left: 4px solid #f44336;
            background: #ffebee;
        }
        .alert.resolved {
            border-left-color: #4caf50;
            background: #e8f5e9;
        }
        .alert-title {
            font-weight: bold;
            margin-bottom: 5px;
        }
        .alert-message {
            color: #666;
            font-size: 0.9em;
        }
        .no-alerts {
            color: #4caf50;
            text-align: center;
            padding: 20px;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-online { background: #4caf50; }
        .status-offline { background: #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🖥️ System Monitoring Dashboard</h1>
        
        <div class="stats-grid">
            <div class="stat-card">
                <div class="stat-label">CPU Usage</div>
                <div class="stat-value" id="cpu-value">0<span class="stat-unit">%</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Memory Usage</div>
                <div class="stat-value" id="memory-value">0<span class="stat-unit">%</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Disk Usage</div>
                <div class="stat-value" id="disk-value">0<span class="stat-unit">%</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Network RX</div>
                <div class="stat-value" id="network-rx-value" style="font-size: 1.8em;">0<span class="stat-unit"> KB/s</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Network TX</div>
                <div class="stat-value" id="network-tx-value" style="font-size: 1.8em;">0<span class="stat-unit"> KB/s</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Status</div>
                <div class="stat-value" style="font-size: 1.5em;">
                    <span class="status-indicator status-online" id="status-indicator"></span>
                    <span id="status-text">Online</span>
                </div>
            </div>
        </div>
        
        <div class="charts-grid">
            <div class="chart-card">
                <div class="chart-title">CPU Usage Over Time</div>
                <canvas id="cpu-chart"></canvas>
            </div>
            <div class="chart-card">
                <div class="chart-title">Memory Usage Over Time</div>
                <canvas id="memory-chart"></canvas>
            </div>
            <div class="chart-card">
                <div class="chart-title">Disk Usage Over Time (%)</div>
                <canvas id="disk-chart"></canvas>
            </div>
            <div class="chart-card">
                <div class="chart-title">Disk I/O Activity</div>
                <canvas id="disk-io-chart"></canvas>
            </div>
            <div class="chart-card">
                <div class="chart-title">Network Traffic</div>
                <canvas id="network-chart"></canvas>
            </div>
        </div>
        
        <div class="stats-grid" style="margin-top: 20px;">
            <div class="stat-card">
                <div class="stat-label">Disk Reads</div>
                <div class="stat-value" id="disk-reads-value" style="font-size: 1.5em;">0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Disk Writes</div>
                <div class="stat-value" id="disk-writes-value" style="font-size: 1.5em;">0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Reads/sec</div>
                <div class="stat-value" id="disk-read-rate-value" style="font-size: 1.5em;">0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Writes/sec</div>
                <div class="stat-value" id="disk-write-rate-value" style="font-size: 1.5em;">0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Data Read</div>
                <div class="stat-value" id="disk-data-read-value" style="font-size: 1.3em;">0<span class="stat-unit"> TB</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Data Written</div>
                <div class="stat-value" id="disk-data-written-value" style="font-size: 1.3em;">0<span class="stat-unit"> TB</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Data Read/sec</div>
                <div class="stat-value" id="disk-data-read-rate-value" style="font-size: 1.3em;">0<span class="stat-unit"> KB/s</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Data Written/sec</div>
                <div class="stat-value" id="disk-data-write-rate-value" style="font-size: 1.3em;">0<span class="stat-unit"> KB/s</span></div>
            </div>
        </div>
        
        <div class="alerts-section">
            <h2 style="margin-bottom: 20px;">Alerts</h2>
            <div id="alerts-container">
                <div class="no-alerts">No active alerts</div>
            </div>
        </div>
    </div>
    
    <script>
        const cpuChart = new Chart(document.getElementById('cpu-chart'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'CPU %',
                    data: [],
                    borderColor: 'rgb(102, 126, 234)',
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                scales: {
                    y: { beginAtZero: true, max: 100 }
                }
            }
        });
        
        const memoryChart = new Chart(document.getElementById('memory-chart'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Memory %',
                    data: [],
                    borderColor: 'rgb(118, 75, 162)',
                    backgroundColor: 'rgba(118, 75, 162, 0.1)',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                scales: {
                    y: { beginAtZero: true, max: 100 }
                }
            }
        });
        
        let diskChartMin = null;
        let diskChartMax = null;
        const diskChart = new Chart(document.getElementById('disk-chart'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Disk Used (%)',
                    data: [],
                    borderColor: 'rgb(255, 99, 132)',
                    backgroundColor: 'rgba(255, 99, 132, 0.1)',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                scales: {
                    y: {
                        beginAtZero: false,
                        min: 0,
                        max: 100
                    }
                },
                plugins: {
                    legend: {
                        display: true
                    }
                }
            }
        });
        
        const diskIOChart = new Chart(document.getElementById('disk-io-chart'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Reads/sec',
                    data: [],
                    borderColor: 'rgb(75, 192, 192)',
                    backgroundColor: 'rgba(75, 192, 192, 0.1)',
                    tension: 0.4
                }, {
                    label: 'Writes/sec',
                    data: [],
                    borderColor: 'rgb(255, 99, 132)',
                    backgroundColor: 'rgba(255, 99, 132, 0.1)',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                scales: {
                    y: { beginAtZero: true }
                }
            }
        });
        
        const networkChart = new Chart(document.getElementById('network-chart'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'RX (MB)',
                    data: [],
                    borderColor: 'rgb(75, 192, 192)',
                    backgroundColor: 'rgba(75, 192, 192, 0.1)',
                    tension: 0.4
                }, {
                    label: 'TX (MB)',
                    data: [],
                    borderColor: 'rgb(255, 159, 64)',
                    backgroundColor: 'rgba(255, 159, 64, 0.1)',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true
            }
        });
        
        let ws = null;
        let reconnectTimeout = null;
        
        function connectWebSocket() {
            // Use ws:// for localhost (browsers don't support wss:// for localhost)
            const wsUrl = (window.location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + window.location.host + '/ws/metrics';
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = () => {
                console.log('WebSocket connected');
                document.getElementById('status-indicator').className = 'status-indicator status-online';
                document.getElementById('status-text').textContent = 'Online';
                if (reconnectTimeout) {
                    clearTimeout(reconnectTimeout);
                    reconnectTimeout = null;
                }
            };
            
            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    updateDashboard(data);
                } catch (e) {
                    console.error('Error parsing WebSocket message:', e);
                }
            };
            
            ws.onerror = (error) => {
                console.error('WebSocket error:', error);
            };
            
            ws.onclose = () => {
                console.log('WebSocket disconnected');
                document.getElementById('status-indicator').className = 'status-indicator status-offline';
                document.getElementById('status-text').textContent = 'Offline';
                
                if (!reconnectTimeout) {
                    reconnectTimeout = setTimeout(connectWebSocket, 3000);
                }
            };
        }
        
        function updateDashboard(data) {
            try {
                // Update stat cards
                if (data.cpu && data.cpu.percent !== undefined) {
                    document.getElementById('cpu-value').innerHTML = data.cpu.percent.toFixed(1) + '<span class="stat-unit">%</span>';
                }
                if (data.memory && data.memory.percent !== undefined) {
                    document.getElementById('memory-value').innerHTML = data.memory.percent.toFixed(1) + '<span class="stat-unit">%</span>';
                }
                if (data.disk && data.disk.percent !== undefined) {
                    document.getElementById('disk-value').innerHTML = data.disk.percent.toFixed(1) + '<span class="stat-unit">%</span>';
                }
                if (data.network) {
                    const rxRateMB = data.network.rx_rate / 1024 / 1024;
                    const txRateMB = data.network.tx_rate / 1024 / 1024;
                    const rxRateKB = data.network.rx_rate / 1024;
                    const txRateKB = data.network.tx_rate / 1024;
                    
                    const rxDisplay = rxRateMB >= 1 ? rxRateMB.toFixed(2) + ' MB/s' : rxRateKB.toFixed(2) + ' KB/s';
                    const txDisplay = txRateMB >= 1 ? txRateMB.toFixed(2) + ' MB/s' : txRateKB.toFixed(2) + ' KB/s';
                    
                    document.getElementById('network-rx-value').innerHTML = rxDisplay;
                    document.getElementById('network-tx-value').innerHTML = txDisplay;
                }
                if (data.disk_io) {
                    // Format large numbers with commas
                    function formatNumber(num) {
                        return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
                    }
                    
                    // Update I/O counts
                    document.getElementById('disk-reads-value').textContent = formatNumber(data.disk_io.reads || 0);
                    document.getElementById('disk-writes-value').textContent = formatNumber(data.disk_io.writes || 0);
                    
                    // Update I/O rates (ops/sec)
                    document.getElementById('disk-read-rate-value').textContent = (data.disk_io.read_rate || 0).toFixed(0);
                    document.getElementById('disk-write-rate-value').textContent = (data.disk_io.write_rate || 0).toFixed(0);
                    
                    // Update data totals (TB)
                    const dataReadTB = (data.disk_io.data_read || 0) / 1024 / 1024 / 1024 / 1024;
                    const dataWrittenTB = (data.disk_io.data_written || 0) / 1024 / 1024 / 1024 / 1024;
                    document.getElementById('disk-data-read-value').innerHTML = dataReadTB.toFixed(2) + '<span class="stat-unit"> TB</span>';
                    document.getElementById('disk-data-written-value').innerHTML = dataWrittenTB.toFixed(2) + '<span class="stat-unit"> TB</span>';
                    
                    // Update data rates (KB/s or MB/s)
                    const dataReadRateMB = (data.disk_io.data_read_rate || 0) / 1024 / 1024;
                    const dataWriteRateMB = (data.disk_io.data_write_rate || 0) / 1024 / 1024;
                    const dataReadRateKB = (data.disk_io.data_read_rate || 0) / 1024;
                    const dataWriteRateKB = (data.disk_io.data_write_rate || 0) / 1024;
                    
                    const dataReadDisplay = dataReadRateMB >= 1 ? dataReadRateMB.toFixed(2) + ' MB/s' : dataReadRateKB.toFixed(2) + ' KB/s';
                    const dataWriteDisplay = dataWriteRateMB >= 1 ? dataWriteRateMB.toFixed(2) + ' MB/s' : dataWriteRateKB.toFixed(2) + ' KB/s';
                    
                    document.getElementById('disk-data-read-rate-value').innerHTML = dataReadDisplay;
                    document.getElementById('disk-data-write-rate-value').innerHTML = dataWriteDisplay;
                }
                
                // Update charts only if they exist
                if (typeof cpuChart !== 'undefined' && typeof memoryChart !== 'undefined' && 
                    typeof diskChart !== 'undefined' && typeof diskIOChart !== 'undefined' && typeof networkChart !== 'undefined') {
                    const now = new Date().toLocaleTimeString();
                    
                    function addData(chart, label, value) {
                        if (chart && chart.data) {
                            chart.data.labels.push(label);
                            chart.data.datasets[0].data.push(value);
                            if (chart.data.labels.length > 60) {
                                chart.data.labels.shift();
                                chart.data.datasets[0].data.shift();
                            }
                            chart.update('none');
                        }
                    }
                    
                    if (data.cpu) addData(cpuChart, now, data.cpu.percent);
                    if (data.memory) addData(memoryChart, now, data.memory.percent);
                    if (data.disk) {
                        // Use percentage for the disk chart so it matches data.disk.percent
                        addData(diskChart, now, data.disk.percent);
                        // Update dynamic range based on current data
                        const chartData = diskChart.data.datasets[0].data;
                        if (chartData.length > 0) {
                            const min = Math.min(...chartData);
                            const max = Math.max(...chartData);
                            const range = max - min;
                            // Set range to show fluctuations: min-2% to max+2%, but at least 5% range
                            diskChart.options.scales.y.min = Math.max(0, min - Math.max(2, range * 0.1));
                            diskChart.options.scales.y.max = Math.min(100, max + Math.max(2, range * 0.1));
                        }
                    }
                    if (data.disk_io && diskIOChart && diskIOChart.data) {
                        diskIOChart.data.labels.push(now);
                        diskIOChart.data.datasets[0].data.push(data.disk_io.read_rate || 0);
                        diskIOChart.data.datasets[1].data.push(data.disk_io.write_rate || 0);
                        if (diskIOChart.data.labels.length > 60) {
                            diskIOChart.data.labels.shift();
                            diskIOChart.data.datasets[0].data.shift();
                            diskIOChart.data.datasets[1].data.shift();
                        }
                        diskIOChart.update('none');
                    }
                    if (data.network && networkChart && networkChart.data) {
                        const rxRateMB = data.network.rx_rate / 1024 / 1024;
                        const txRateMB = data.network.tx_rate / 1024 / 1024;
                        networkChart.data.labels.push(now);
                        networkChart.data.datasets[0].data.push(rxRateMB);
                        networkChart.data.datasets[1].data.push(txRateMB);
                        if (networkChart.data.labels.length > 60) {
                            networkChart.data.labels.shift();
                            networkChart.data.datasets[0].data.shift();
                            networkChart.data.datasets[1].data.shift();
                        }
                        networkChart.update('none');
                    }
                }
            } catch (e) {
                console.error('Error in updateDashboard:', e);
            }
        }
        
        function updateAlerts() {
            fetch('/api/alerts')
                .then(res => res.json())
                .then(alerts => {
                    const container = document.getElementById('alerts-container');
                    if (alerts.length === 0) {
                        container.innerHTML = '<div class="no-alerts">No active alerts</div>';
                    } else {
                        container.innerHTML = alerts.map(alert => `
                            <div class="alert">
                                <div class="alert-title">${alert.type}</div>
                                <div class="alert-message">${alert.message} (Current: ${alert.current_value.toFixed(2)}, Threshold: ${alert.threshold})</div>
                            </div>
                        `).join('');
                    }
                })
                .catch(err => console.error('Error fetching alerts:', err));
        }
        
        // Initial load
        fetch('/api/metrics/latest')
            .then(res => res.json())
            .then(data => updateDashboard(data))
            .catch(err => console.error('Error fetching initial metrics:', err));
        
        updateAlerts();
        setInterval(updateAlerts, 5000);
        
        // Connect WebSocket
        connectWebSocket();
    </script>
</body>
</html>)";
}

int main(int argc, char* argv[]) {
    Config config;
    if (argc > 1) {
        config.port = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    if (argc > 2) {
        config.thread_pool_size = std::stoul(argv[2]);
    }
    
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    try {
        // Initialize monitoring components
        MetricsCollector collector;
        MetricsStorage storage(3600); // Store 1 hour of data
        AlertManager alert_manager;
        
        // Set alert thresholds
        alert_manager.SetCpuThreshold(80.0);
        alert_manager.SetMemoryThreshold(85.0);
        alert_manager.SetDiskThreshold(90.0);
        alert_manager.SetNetworkThreshold(100 * 1024 * 1024); // 100 MB/s
        
        // Set alert callback
        alert_manager.SetAlertCallback([](const Alert& alert) {
            std::cout << "[ALERT] " << alert.message 
                      << " (Current: " << alert.current_value 
                      << ", Threshold: " << alert.threshold << ")\n";
        });
        
        g_collector = &collector;
        g_storage = &storage;
        g_alert_manager = &alert_manager;
        
        Server server(config);
        g_server = &server;
        
        // Dashboard
        server.Get("/", [](const HttpRequest& req) {
            HttpResponse response(HttpStatus::OK);
            response.SetContentType("text/html");
            response.SetBody(GetDashboardHTML());
            return response;
        });
        
        // API: Get latest metrics
        server.Get("/api/metrics/latest", [&storage](const HttpRequest& req) {
            SystemMetrics latest = storage.GetLatest();
            return JsonResponse(latest.ToJson());
        });
        
        // API: Get metrics for time range
        server.Get("/api/metrics/range", [&storage](const HttpRequest& req) {
            auto now = std::chrono::system_clock::now();
            size_t seconds = 300; // Default 5 minutes
            
            if (req.query_params.count("seconds")) {
                seconds = std::stoul(req.query_params.at("seconds"));
            }
            
            auto samples = storage.GetLastSeconds(seconds);
            
            std::ostringstream json;
            json << "[";
            for (size_t i = 0; i < samples.size(); ++i) {
                json << samples[i].ToJson();
                if (i < samples.size() - 1) json << ",";
            }
            json << "]";
            
            return JsonResponse(json.str());
        });
        
        // API: Get aggregated stats
        server.Get("/api/metrics/stats", [&storage](const HttpRequest& req) {
            auto now = std::chrono::system_clock::now();
            size_t seconds = 3600; // Default 1 hour
            
            if (req.query_params.count("seconds")) {
                seconds = std::stoul(req.query_params.at("seconds"));
            }
            
            auto start = now - std::chrono::seconds(seconds);
            auto stats = storage.GetAggregatedStats(start, now);
            
            std::ostringstream json;
            json << std::fixed << std::setprecision(2);
            json << "{"
                 << "\"avg_cpu\":" << stats.avg_cpu << ","
                 << "\"max_cpu\":" << stats.max_cpu << ","
                 << "\"min_cpu\":" << stats.min_cpu << ","
                 << "\"avg_memory\":" << stats.avg_memory << ","
                 << "\"max_memory\":" << stats.max_memory << ","
                 << "\"min_memory\":" << stats.min_memory << ","
                 << "\"total_network_rx\":" << stats.total_network_rx << ","
                 << "\"total_network_tx\":" << stats.total_network_tx
                 << "}";
            
            return JsonResponse(json.str());
        });
        
        // API: Get active alerts
        server.Get("/api/alerts", [&alert_manager](const HttpRequest& req) {
            auto alerts = alert_manager.GetActiveAlerts();
            
            std::ostringstream json;
            json << "[";
            for (size_t i = 0; i < alerts.size(); ++i) {
                json << alerts[i].ToJson();
                if (i < alerts.size() - 1) json << ",";
            }
            json << "]";
            
            return JsonResponse(json.str());
        });
        
        // WebSocket endpoint for real-time metrics
        server.RegisterWebSocketHandler("/ws/metrics", [&storage](int client_fd, const std::string&) {
            // Send metrics every second
            std::thread([client_fd, &storage]() {
                while (g_running) {
                    // Check if connection is still valid
                    if (fcntl(client_fd, F_GETFL) < 0) {
                        break; // Connection closed
                    }
                    
                    SystemMetrics latest = storage.GetLatest();
                    
                    // Check if we have valid data
                    if (latest.memory_total == 0 && latest.cpu_percent == 0) {
                        // No data collected yet, wait a bit
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        continue;
                    }
                    
                    std::string json = latest.ToJson();
                    auto frame = WebSocket::EncodeFrame(json, WebSocket::Opcode::Text);
                    ssize_t sent = send(client_fd, frame.data(), frame.size(), 0);
                    
                    if (sent < 0) {
                        // Error sending, connection probably closed
                        break;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                
                // Clean up
                if (fcntl(client_fd, F_GETFL) >= 0) {
                    close(client_fd);
                }
            }).detach();
        });
        
        // Health check
        server.Get("/health", [](const HttpRequest& req) {
            return JsonResponse(R"({"status": "healthy", "service": "monitoring-server"})");
        });
        
        // Start metrics collection thread
        std::thread metrics_thread(MetricsCollectionThread);
        metrics_thread.detach();
        
        std::cout << "🚀 System Monitoring Server starting on port " << config.port << "\n";
        std::cout << "📊 Dashboard: http://localhost:" << config.port << "/\n";
        std::cout << "📡 WebSocket: ws://localhost:" << config.port << "/ws/metrics\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        
        server.Start();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
