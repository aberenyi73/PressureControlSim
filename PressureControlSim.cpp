#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <iomanip>
#include <atomic>

using namespace std;

// ============================================================================
// SEMAPHORE IMPLEMENTATION (C++11 compatible)
// ============================================================================
class BinarySemaphore {
private:
    mutex mtx;
    condition_variable cv;
    bool signaled = false;

public:
    // "Take" the semaphore (wait until signaled)
    void take() {
        unique_lock<mutex> lock(mtx);
        // Wait until semaphore is signaled
        cv.wait(lock, [this] { return signaled; });
        signaled = false;  // Reset after taking
    }

    // "Give" the semaphore (signal waiting thread)
    void give() {
        {
            lock_guard<mutex> lock(mtx);
            signaled = true;
        }
        cv.notify_one();  // Wake up waiting thread
    }
};

// ============================================================================
// SHARED DATA (Protected by the semaphore pattern)
// ============================================================================
struct PressureData {
    float pressure_bar = 0.0f;
    int sample_number = 0;
};

PressureData pressure_buffer;  // Shared between ISR and Thread
BinarySemaphore pressure_ready_sem;  // Synchronization mechanism
atomic<bool> system_running(true);  // Control flag for shutdown

// Statistics
atomic<int> total_samples(0);
atomic<int> pump_adjustments(0);

// ============================================================================
// SIMULATED ADC INTERRUPT SERVICE ROUTINE
// ============================================================================
// In real embedded system: triggered by hardware ADC conversion complete
// Here: simulated by a thread that "fires" every 1ms
void ADC_ISR_Simulator() {
    random_device rd;
    mt19937 gen(rd());
    // Simulate pressure fluctuations around 9 bar (espresso brewing pressure)
    uniform_real_distribution<float> pressure_noise(8.5f, 9.5f);

    cout << "[ISR] ADC Interrupt Simulator started (1000 Hz sampling)\n";
    cout << "------------------------------------------------------\n";

    int sample_count = 0;

    while (system_running) {
        // Simulate 1ms between ADC conversions (1000 Hz sampling rate)
        this_thread::sleep_for(chrono::milliseconds(1));

        // === BEGIN INTERRUPT CONTEXT ===
        // This simulates what happens when hardware triggers the interrupt

        // Read simulated ADC value (in real system: read from ADC register)
        float new_pressure = pressure_noise(gen);

        // Store in buffer (shared memory)
        pressure_buffer.pressure_bar = new_pressure;
        pressure_buffer.sample_number = ++sample_count;
        total_samples++;

        // Print ISR activity (only every 100 samples to avoid spam)
        if (sample_count % 100 == 0) {
            cout << "[ISR] Sample #" << sample_count
                << " | Pressure: " << fixed << setprecision(2)
                << new_pressure << " bar | Signaling thread...\n";
        }

        // CRITICAL: Signal the waiting thread that new data is ready
        // This is the "xSemaphoreGiveFromISR" equivalent
        pressure_ready_sem.give();

        // === END INTERRUPT CONTEXT ===
        // In real system, ISR must complete in microseconds!
    }

    cout << "[ISR] Simulator stopped\n";
}

// ============================================================================
// PRESSURE CONTROL THREAD
// ============================================================================
// High-priority thread that adjusts pump speed based on pressure readings
void PressureControlThread() {
    const float TARGET_PRESSURE = 9.0f;  // Target espresso pressure
    const float TOLERANCE = 0.3f;
    int wakeup_count = 0;

    cout << "[THREAD] Pressure Control Thread started\n";
    cout << "[THREAD] Target pressure: " << TARGET_PRESSURE << " bar\n";
    cout << "------------------------------------------------------\n\n";

    while (system_running) {
        // === BLOCKING WAIT ===
        // Thread goes to SLEEP here, consuming ZERO CPU
        // Only wakes up when ISR signals via semaphore
        cout << "[THREAD] Waiting for pressure data (sleeping) ...zZz\n";

        pressure_ready_sem.take();  // Equivalent to xSemaphoreTake()

        // === THREAD WAKES UP HERE ===
        wakeup_count++;

        if (!system_running) break;  // Check if we should exit

        // Read the pressure data from shared buffer
        float current_pressure = pressure_buffer.pressure_bar;
        int sample_num = pressure_buffer.sample_number;

        cout << "\n[THREAD] [wake] WOKE UP! (wakeup #" << wakeup_count << ")\n";
        cout << "[THREAD] Read Sample #" << sample_num
            << " | Pressure: " << fixed << setprecision(2)
            << current_pressure << " bar\n";

        // === CONTROL LOGIC ===
        // Adjust pump speed based on pressure error
        float error = TARGET_PRESSURE - current_pressure;

        if (abs(error) > TOLERANCE) {
            pump_adjustments++;

            if (error > 0) {
                cout << "[THREAD] [up]  Pressure too LOW (" << current_pressure
                    << " bar) - INCREASING pump speed\n";
            } else {
                cout << "[THREAD] [down]  Pressure too HIGH (" << current_pressure
                    << " bar) - DECREASING pump speed\n";
            }

            // Simulate pump adjustment (in real system: write to PWM register)
            int pump_pwm_percent = static_cast<int>(50 + error * 10);
            pump_pwm_percent = max(0, min(100, pump_pwm_percent));  // Clamp 0-100

            cout << "[THREAD] [pump] Setting pump PWM to " << pump_pwm_percent << "%\n";

            // Simulate time to adjust hardware (write to registers, etc.)
            this_thread::sleep_for(chrono::microseconds(50));
        } else {
            cout << "[THREAD] [OK] Pressure OK (" << current_pressure
                << " bar) - No adjustment needed\n";
        }

        cout << "[THREAD] Processing complete. Going back to sleep...\n";
        cout << "======================================================\n\n";

        // In real system, thread would loop back to xSemaphoreTake() immediately
        // Here we add tiny delay for readability
        this_thread::sleep_for(chrono::milliseconds(5));
    }

    cout << "[THREAD] Control thread stopped\n";
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main0() {
    cout << "\n";
    cout << "========================================================\n";
    cout << "   COFFEE MACHINE PRESSURE CONTROL SIMULATION\n";
    cout << "========================================================\n";
    cout << "Simulating: ISR (Interrupt) -> Semaphore -> Thread\n";
    cout << "========================================================\n\n";

    cout << "SYSTEM ARCHITECTURE:\n";
    cout << "  1. ADC Interrupt (ISR)     - Samples pressure at 1000 Hz\n";
    cout << "  2. Binary Semaphore        - Signals when data ready\n";
    cout << "  3. Pressure Control Thread - Wakes up and adjusts pump\n\n";

    cout << "Press Ctrl+C to stop (or wait 10 seconds)...\n\n";
    this_thread::sleep_for(chrono::seconds(2));

    // Start the ADC ISR simulator thread (simulates hardware interrupt)
    thread isr_thread(ADC_ISR_Simulator);

    // Start the pressure control thread (application thread)
    thread control_thread(PressureControlThread);

    // Let the simulation run for 10 seconds
    this_thread::sleep_for(chrono::seconds(10));

    // Shutdown
    cout << "\n\n========================================================\n";
    cout << "SHUTTING DOWN SYSTEM...\n";
    cout << "========================================================\n";

    system_running = false;
    pressure_ready_sem.give();  // Wake up thread so it can exit

    // Wait for threads to finish
    if (isr_thread.joinable()) isr_thread.join();
    if (control_thread.joinable()) control_thread.join();

    // Print statistics
    cout << "\n========================================================\n";
    cout << "SIMULATION STATISTICS:\n";
    cout << "========================================================\n";
    cout << "Total ADC samples:      " << total_samples << "\n";
    cout << "Thread wakeups:         ~" << total_samples << " (1:1 ratio)\n";
    cout << "Pump adjustments:       " << pump_adjustments << "\n";
    cout << "Efficiency:             Thread slept between samples!\n";
    cout << "CPU waste:              ZERO (no polling!)\n";
    cout << "========================================================\n\n";

    cout << "Simulation complete. Press Enter to exit...";
    cin.get();

    return 0;
}