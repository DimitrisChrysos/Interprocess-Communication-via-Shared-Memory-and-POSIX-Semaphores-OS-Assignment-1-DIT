# Interprocess Communication via Shared Memory and POSIX Semaphores

This project implements interprocess communication between two C programs using System V shared memory and POSIX unnamed semaphores. It features full-duplex message transmission and includes statistics reporting on termination.

## Overview

The system consists of two C programs:
- `process_a.c`
- `process_b.c`

Both programs:
- Use shared memory (`struct shared_use_st`) to exchange data.
- Spawn two threads:
  - `send_thread`: Captures user input and sends it as 15-character packets.
  - `receive_thread`: Reconstructs incoming packets into full messages and prints them.
- Maintain usage statistics through a nested `statistics` struct.

## Features

- Thread-based message sending/receiving.
- Safe memory access via POSIX semaphores:
  - `wait_A_receive` / `wait_B_receive`: Controls when the receiver can read.
  - `constr_msgA` / `constr_msgB`: Ensures packet-wise send-receive synchronization.
- Accurate time tracking using `gettimeofday()` to compute average wait time for the first packet of a new message.
- Graceful shutdown and memory cleanup.

## Usage

Compile each process using:
```bash
gcc -o process_a process_a.c -lpthread
gcc -o process_b process_b.c -lpthread
```

Run each in a separate terminal:
```bash
./process_a
./process_b
```

Send messages by typing in one terminal and watching them appear in the other.

To terminate, type:
```
#BYE#
```

## Statistics

Upon termination, the program prints:
- Total messages exchanged
- Average wait time for the first packet (in microseconds)
- Any additional metrics stored in the `statistics` struct

## Dependencies

- POSIX Threads
- System V Shared Memory
- POSIX Semaphores
- Linux-based system recommended

## Author

Δημήτριος Χρυσός

---

This project was built as part of a university assignment to explore low-level synchronization and communication primitives in C.
