# PressureControlSim

How to Run This Code
In Visual Studio:

Create new project:

File → New → Project
Select "Console App" (C++)
Name it "PressureControlSim"


Copy the code:

Replace the entire contents of the .cpp file with the code above


Build and run:

Press F5 or click "Local Windows Debugger"
Watch the output!




# What You'll See
The output will look like this:
========================================================
   COFFEE MACHINE PRESSURE CONTROL SIMULATION
========================================================

[ISR] Sample #100 | Pressure: 9.23 bar | Signaling thread...

[THREAD] ⏰ WOKE UP! (wakeup #100)
[THREAD] Read Sample #100 | Pressure: 9.23 bar
[THREAD] ⬇️  Pressure too HIGH (9.23 bar) - DECREASING pump speed
[THREAD] 🔧 Setting pump PWM to 47%
[THREAD] Going back to sleep...

[THREAD] Waiting for pressure data (sleeping)... 💤

[ISR] Sample #200 | Pressure: 8.67 bar | Signaling thread...

[THREAD] ⏰ WOKE UP! (wakeup #200)
[THREAD] Read Sample #200 | Pressure: 8.67 bar
[THREAD] ⬆️  Pressure too LOW (8.67 bar) - INCREASING pump speed
[THREAD] 🔧 Setting pump PWM to 53%

# Key Concepts Demonstrated
1. Thread Sleeps Efficiently
cpppressure_ready_sem.take();  // Thread BLOCKS here (uses 0% CPU)
The thread isn't checking "is data ready? is data ready?" in a loop. It's asleep until the ISR wakes it up.
2. ISR Signals the Thread
cpppressure_ready_sem.give();  // Wake up the sleeping thread!
When new pressure data arrives, the ISR "rings the bell" to wake the thread.
3. One-to-One Communication
For every pressure sample the ISR reads, the thread wakes up once. No data is missed, no CPU is wasted.
4. Real-World Timing

ISR fires every 1 millisecond (1000 Hz)
Thread processes each sample
This simulates real embedded system timing


# Comparison: With vs Without Semaphore
## WITHOUT Semaphore (Polling - BAD):
<code>
// Thread constantly checking - wastes CPU!
while(true) {
    if(data_ready_flag) {  // Checking... checking... checking...
        ProcessData();
        data_ready_flag = false;
    }
    // This loop runs MILLIONS of times per second! 🔥
}
</code>
CPU usage: 100% spinning!

## WITH Semaphore (Good - What the simulation does):
<code>
while(true) {
    pressure_ready_sem.take();  // Sleep until signaled
    ProcessData();  // Only runs when there's actual work!
}
</code>
CPU usage while waiting: 0%

# Try These Experiments
## Experiment 1: Change Sample Rate
<code>
// In ADC_ISR_Simulator(), change this line:
this_thread::sleep_for(chrono::milliseconds(1));  // 1000 Hz

// To this:
this_thread::sleep_for(chrono::milliseconds(10));  // 100 Hz
Watch how the thread wakes up less frequently!
</code>

## Experiment 2: Add Processing Delay
<code>
// In PressureControlThread(), add this after waking up:
this_thread::sleep_for(chrono::milliseconds(5));  // Simulate slow processing
</code>  
See how samples might queue up if processing is too slow!

## Experiment 3: Remove the Semaphore (See the Bad Way)
Comment out the semaphore and use a flag instead - you'll see constant checking!

### Real Embedded System Differences
This simulation is faithful to the concept, but real embedded systems have:

True hardware interrupts - Not simulated threads
Microsecond timing - Not milliseconds
RTOS primitives - FreeRTOS, ThreadX, etc.
Direct register access - No cout in real ISRs!
Priority preemption - Higher priority truly interrupts lower

But the semaphore pattern is identical - this is exactly how ISR-to-thread communication works! 🎯
