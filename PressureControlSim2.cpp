#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <vector>
#include <atomic>
#include <iomanip>

using namespace std;

// ============================================================================
// BINARY SEMAPHORE (for ISR → Thread signaling)
// ============================================================================
class BinarySemaphore {
private:
    mutex mtx;
    condition_variable cv;
    bool signaled = false;
public:
    void give() {
        lock_guard<mutex> lock(mtx);
        signaled = true;
        cv.notify_one();
    }
    void take() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] { return signaled; });
        signaled = false;
    }
};

// ============================================================================
// SHARED DATA STRUCTURES
// ============================================================================

// Circular buffer protected by mutex (multiple threads access this)
struct CircularBuffer {
    static const int SIZE = 10;
    float data[SIZE];
    int write_idx = 0;
    int read_idx = 0;
    int count = 0;

    mutex buffer_mutex;  //  MUTEX protects this buffer

    // Producer: Add data (called by ISR handler thread)
    bool Push(float value) {
        lock_guard<mutex> lock(buffer_mutex);  //  Lock

        if (count >= SIZE) {
            return false;  // Buffer full
        }

        data[write_idx] = value;
        write_idx = (write_idx + 1) % SIZE;
        count++;
        return true;
    }  // 🔓 Auto-unlock when lock goes out of scope

    // Consumer: Get data (called by processing thread)
    bool Pop(float& value) {
        lock_guard<mutex> lock(buffer_mutex);  //  Lock

        if (count == 0) {
            return false;  // Buffer empty
        }

        value = data[read_idx];
        read_idx = (read_idx + 1) % SIZE;
        count--;
        return true;
    }  // Auto-unlock

    int GetCount() {
        lock_guard<mutex> lock(buffer_mutex);
        return count;
    }
};

CircularBuffer pressure_buffer;     // Shared between multiple threads
BinarySemaphore new_data_semaphore; // ISR signals this
atomic<bool> system_running(true);

// Statistics (atomic = thread-safe without mutex)
atomic<int> samples_produced(0);
atomic<int> samples_consumed(0);
atomic<int> buffer_overflows(0);

// ============================================================================
// SIMULATED ISR (Producer)
// ============================================================================
// Simulates hardware interrupt that occurs every 1ms
void ISR_Simulator() {
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<float> pressure_dist(8.0f, 10.0f);

    cout << "[ISR THREAD] Started - Producing data every 1ms\n\n";

    while (system_running) {
        this_thread::sleep_for(chrono::milliseconds(1));  // Simulate 1kHz

        // === BEGIN SIMULATED ISR ===
        float pressure = pressure_dist(gen);

        // Try to add to buffer (mutex used inside Push)
        if (pressure_buffer.Push(pressure)) {
            samples_produced++;

            // Signal consumer thread: "New data available!"
            new_data_semaphore.give();  // Ring the bell

            if (samples_produced % 500 == 0) {
                cout << "[ISR] Produced sample #" << samples_produced
                    << " | Pressure: " << fixed << setprecision(2)
                    << pressure << " bar | Buffer: "
                    << pressure_buffer.GetCount() << "/"
                    << CircularBuffer::SIZE << "\n";
            }
        } else {
            buffer_overflows++;
            cout << "[ISR]  BUFFER FULL! Sample dropped (overflow #"
                << buffer_overflows << ")\n";
        }
        // === END SIMULATED ISR ===
    }

    cout << "[ISR THREAD] Stopped\n";
}

// ============================================================================
// PROCESSING THREAD (Consumer)
// ============================================================================
// Waits for semaphore signal, then processes buffered data
void ProcessingThread() {
    cout << "[PROCESSING THREAD] Started - Waiting for data\n\n";

    while (system_running) {
        // === WAIT FOR SEMAPHORE (sleeps here) ===
        new_data_semaphore.take();  // Sleep until ISR signals

        if (!system_running) break;

        // === WOKE UP! Process all available data ===
        float pressure;
        int processed_count = 0;

        // Pop all available samples from buffer
        while (pressure_buffer.Pop(pressure)) {  // Mutex used inside Pop
            samples_consumed++;
            processed_count++;

            // Simulate processing (PID control, etc.)
            // In real system: adjust pump PWM, update display, etc.

            if (samples_consumed % 500 == 0) {
                cout << "[PROCESSING] Consumed sample #" << samples_consumed
                    << " | Pressure: " << fixed << setprecision(2)
                    << pressure << " bar\n";
            }
        }

        if (processed_count > 1) {
            cout << "[PROCESSING] Processed " << processed_count
                << " samples in one wake-up (batch processing)\n";
        }
    }

    cout << "[PROCESSING THREAD] Stopped\n";
}

// ============================================================================
// MONITORING THREAD (Another Consumer)
// ============================================================================
// Periodically checks buffer status (demonstrates multiple threads using mutex)
void MonitoringThread() {
    cout << "[MONITORING THREAD] Started - Checking buffer every 2s\n\n";

    while (system_running) {
        this_thread::sleep_for(chrono::seconds(2));

        // Access shared buffer (mutex protects this)
        int buffer_level = pressure_buffer.GetCount();

        float usage_percent = (buffer_level * 100.0f) / CircularBuffer::SIZE;

        cout << "\n[MONITOR] === STATUS CHECK ===\n";
        cout << "[MONITOR] Buffer usage: " << buffer_level << "/"
            << CircularBuffer::SIZE << " (" << fixed << setprecision(1)
            << usage_percent << "%)\n";
        cout << "[MONITOR] Produced: " << samples_produced
            << " | Consumed: " << samples_consumed
            << " | Overflows: " << buffer_overflows << "\n";
        cout << "[MONITOR] ========================\n\n";
    }

    cout << "[MONITORING THREAD] Stopped\n";
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main() {
    cout << "\n";
    cout << "================================================================\n";
    cout << "     MUTEX + SEMAPHORE COLLABORATION DEMONSTRATION\n";
    cout << "================================================================\n";
    cout << "\nArchitecture:\n";
    cout << "  1. ISR Simulator (Producer)   - Generates pressure data at 1kHz\n";
    cout << "  2. Circular Buffer             - Protected by MUTEX 🔑\n";
    cout << "  3. Binary Semaphore            - Signals new data 🔔\n";
    cout << "  4. Processing Thread (Consumer)- Wakes on semaphore, uses mutex\n";
    cout << "  5. Monitoring Thread           - Also uses mutex to check buffer\n";
    cout << "\nKey Concepts:\n";
    cout << "  • SEMAPHORE signals between ISR and thread (notification)\n";
    cout << "  • MUTEX protects buffer from simultaneous access (protection)\n";
    cout << "  • Multiple threads safely share the buffer using the same mutex\n";
    cout << "\n================================================================\n\n";

    cout << "Running for 10 seconds...\n\n";
    this_thread::sleep_for(chrono::seconds(1));

    // Start all threads
    thread isr_thread(ISR_Simulator);
    thread processing_thread(ProcessingThread);
    thread monitoring_thread(MonitoringThread);

    // Run simulation
    this_thread::sleep_for(chrono::seconds(10));

    // Shutdown
    cout << "\n================================================================\n";
    cout << "SHUTTING DOWN...\n";
    cout << "================================================================\n";

    system_running = false;
    new_data_semaphore.give();  // Wake up processing thread to exit

    if (isr_thread.joinable()) isr_thread.join();
    if (processing_thread.joinable()) processing_thread.join();
    if (monitoring_thread.joinable()) monitoring_thread.join();

    // Final statistics
    cout << "\n================================================================\n";
    cout << "FINAL STATISTICS:\n";
    cout << "================================================================\n";
    cout << "Samples produced:       " << samples_produced << "\n";
    cout << "Samples consumed:       " << samples_consumed << "\n";
    cout << "Buffer overflows:       " << buffer_overflows << "\n";
    cout << "Data loss:              " << (samples_produced - samples_consumed) << " samples\n";
    cout << "================================================================\n\n";

    // Show what would happen WITHOUT mutex
    cout << "[?] What if we DIDN'T use a mutex?\n";
    cout << "   → ISR and Processing threads would both access buffer\n";
    cout << "   → Race conditions: corrupted indices, lost data, crashes!\n";
    cout << "   → The mutex ensures only ONE thread touches buffer at a time\n\n";

    // Show what would happen WITHOUT semaphore
    cout << "[?] What if we DIDN'T use a semaphore?\n";
    cout << "   → Processing thread would busy-wait (waste CPU)\n";
    cout << "   → Or sleep randomly (miss data or add latency)\n";
    cout << "   → The semaphore allows efficient event-driven processing\n\n";

    cout << "Press Enter to exit...";
    cin.get();

    return 0;
}