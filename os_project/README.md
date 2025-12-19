# OS Scheduler Simulation

This folder contains the ENCS3390 Project 2 implementation written in Python.

## Files

- `os_project.py` — simulator entry point.
- `scenario_deadlock.txt` — complex scenario with ≥5 processes/bursts that produces a deadlock. The simulator will detect it and terminate the lowest-priority process to recover.
- `scenario_aging.txt` — complex scenario that provokes long waiting times so that the aging mechanism promotes a low-priority job.

## Running the Simulator

```bash
python3 os_project.py scenario_deadlock.txt
python3 os_project.py scenario_aging.txt
```

Each run prints the Gantt chart, average waiting/turnaround times, and any deadlock events.
