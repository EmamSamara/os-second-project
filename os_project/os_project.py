"""Operating System Scheduler Simulation for ENCS3390 Project 2.

The implementation models:
    * Preemptive priority scheduling with round-robin time slicing per priority.
    * Aging (priority promotion) for tasks waiting too long in the ready queue.
    * Resource requests/releases embedded inside CPU bursts.
    * I/O bursts that proceed in parallel.
    * Deadlock detection and a basic recovery strategy (terminate lowest-priority
      process participating in the deadlock).
    * Final report with Gantt chart data, average waiting/turnaround time, and
      a log of deadlock events.

Input file format:
    [ResourceID, Instances], [ResourceID, Instances], ...
    PID Arrival Priority BURST_SPEC ...

Example burst specification:
    CPU {R[1,2], 50, F[1,1], 20}
    IO {30}

CPU bursts may contain operations:
    - R[resource_id, instances] : request resource
    - F[resource_id, instances] : free resource
    - numbers (e.g., 50) : execute for N time units

Every process must start and end with CPU bursts.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from collections import deque
from typing import Deque, Dict, List, Optional, Tuple

TIME_QUANTUM = 5
AGING_THRESHOLD = 10
MAX_PRIORITY = 20


@dataclass
class CPUAction:
    kind: str  # "run", "request", "release"
    value: int
    amount: int = 0


@dataclass
class Burst:
    kind: str  # "CPU" or "IO"
    actions: List[CPUAction] = field(default_factory=list)
    duration: int = 0  # used for IO bursts


@dataclass
class Process:
    pid: int
    arrival: int
    priority: int
    bursts: List[Burst]

    current_burst_index: int = 0
    current_action_index: int = 0
    action_remaining: int = 0
    io_remaining: int = 0
    waiting_time: int = 0
    turnaround: int = 0
    ready_wait_counter: int = 0
    quantum_remaining: int = TIME_QUANTUM
    start_time: Optional[int] = None
    finish_time: Optional[int] = None
    state: str = "new"  # new, ready, running, waiting, io, finished, terminated
    pending_request: Optional[Tuple[int, int]] = None  # (resource_id, amount)

    def current_burst(self) -> Optional[Burst]:
        if 0 <= self.current_burst_index < len(self.bursts):
            return self.bursts[self.current_burst_index]
        return None

    def has_more_bursts(self) -> bool:
        return self.current_burst_index < len(self.bursts)

    def reset_quantum(self) -> None:
        self.quantum_remaining = TIME_QUANTUM


class ResourceManager:
    def __init__(self, totals: Dict[int, int]) -> None:
        self.total = totals
        self.available = dict(totals)
        self.allocated: Dict[int, Dict[int, int]] = {}

    def _ensure_pid(self, pid: int) -> None:
        if pid not in self.allocated:
            self.allocated[pid] = {}

    def request(self, pid: int, rid: int, amount: int) -> bool:
        if self.available.get(rid, 0) >= amount:
            self.available[rid] -= amount
            self._ensure_pid(pid)
            self.allocated[pid][rid] = self.allocated[pid].get(rid, 0) + amount
            return True
        return False

    def release(self, pid: int, rid: int, amount: int) -> None:
        self._ensure_pid(pid)
        held = self.allocated[pid].get(rid, 0)
        freed = min(amount, held)
        if freed:
            self.allocated[pid][rid] = held - freed
            self.available[rid] = self.available.get(rid, 0) + freed

    def release_all(self, pid: int) -> None:
        for rid, amount in self.allocated.get(pid, {}).items():
            self.available[rid] = self.available.get(rid, 0) + amount
        self.allocated[pid] = {}


def parse_resources(line: str) -> Dict[int, int]:
    resources: Dict[int, int] = {}
    for match in re.finditer(r"\[(\d+),\s*(\d+)\]", line):
        rid = int(match.group(1))
        count = int(match.group(2))
        resources[rid] = count
    return resources


def parse_burst(token: str) -> Burst:
    token = token.strip()
    if token.startswith("CPU"):
        actions_str = re.search(r"\{(.*)\}", token).group(1)
        parts = [part.strip() for part in actions_str.split(",")]
        actions: List[CPUAction] = []
        for part in parts:
            if not part:
                continue
            if part.startswith("R["):
                rid, amt = map(int, re.findall(r"\d+", part))
                actions.append(CPUAction("request", rid, amt))
            elif part.startswith("F["):
                rid, amt = map(int, re.findall(r"\d+", part))
                actions.append(CPUAction("release", rid, amt))
            else:
                duration = int(part)
                actions.append(CPUAction("run", duration))
        return Burst(kind="CPU", actions=actions)
    elif token.startswith("IO"):
        duration = int(re.search(r"\{(\d+)\}", token).group(1))
        return Burst(kind="IO", duration=duration)
    raise ValueError(f"Unknown burst token: {token}")


def parse_process(line: str) -> Process:
    parts = line.strip().split()
    pid = int(parts[0])
    arrival = int(parts[1])
    priority = int(parts[2])
    burst_tokens = " ".join(parts[3:])
    bursts = []
    for match in re.finditer(r"(CPU\s*\{[^}]+\}|IO\s*\{[^}]+\})", burst_tokens):
        bursts.append(parse_burst(match.group(1)))
    return Process(pid=pid, arrival=arrival, priority=priority, bursts=bursts)


def load_processes(file_path: str) -> Tuple[ResourceManager, List[Process]]:
    with open(file_path, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]
    resources = parse_resources(lines[0])
    processes = [parse_process(line) for line in lines[1:]]
    return ResourceManager(resources), processes


class Scheduler:
    def __init__(self, rm: ResourceManager, processes: List[Process]) -> None:
        self.rm = rm
        self.processes = {p.pid: p for p in processes}
        self.ready_queues: Dict[int, Deque[Process]] = {
            prio: deque() for prio in range(MAX_PRIORITY + 1)
        }
        self.io_queue: List[Process] = []
        self.waiting_for_resources: List[Process] = []
        self.time = 0
        self.running: Optional[Process] = None
        self.gantt: List[Tuple[int, int, Optional[int]]] = []
        self.deadlock_log: List[str] = []
        self.completed = 0
        self.total_processes = len(processes)
        self.last_gantt_pid: Optional[int] = None
        self.gantt_start_time = 0

    def log_gantt(self, pid: Optional[int]) -> None:
        if pid == self.last_gantt_pid:
            return
        if self.last_gantt_pid is not None:
            self.gantt.append((self.gantt_start_time, self.time, self.last_gantt_pid))
        self.last_gantt_pid = pid
        self.gantt_start_time = self.time

    def finalize_gantt(self) -> None:
        if self.last_gantt_pid is not None:
            self.gantt.append((self.gantt_start_time, self.time, self.last_gantt_pid))

    def add_to_ready(self, proc: Process) -> None:
        proc.state = "ready"
        proc.reset_quantum()
        self.ready_queues[proc.priority].append(proc)

    def pick_next_process(self) -> Optional[Process]:
        for priority in range(MAX_PRIORITY + 1):
            queue = self.ready_queues[priority]
            if queue:
                proc = queue.popleft()
                proc.state = "running"
                if proc.start_time is None:
                    proc.start_time = self.time
                return proc
        return None

    def aging(self) -> None:
        for priority, queue in self.ready_queues.items():
            new_queue = deque()
            while queue:
                proc = queue.popleft()
                proc.ready_wait_counter += 1
                if proc.ready_wait_counter >= AGING_THRESHOLD and proc.priority > 0:
                    proc.priority -= 1
                    proc.ready_wait_counter = 0
                new_queue.append(proc)
            self.ready_queues[priority] = new_queue
        # After adjusting priorities, re-bucket processes
        all_ready = []
        for q in self.ready_queues.values():
            all_ready.extend(q)
            q.clear()
        for proc in all_ready:
            self.ready_queues[proc.priority].append(proc)

    def schedule(self) -> None:
        best_proc = self.peek_best_ready()
        if not self.running:
            if best_proc:
                self.running = self.pop_ready(best_proc.priority)
                self.log_gantt(self.running.pid)
            else:
                self.log_gantt(None)
            return

        if best_proc and best_proc.priority < self.running.priority:
            # Preempt current running process
            current = self.running
            current.state = "ready"
            self.ready_queues[current.priority].append(current)
            self.running = self.pop_ready(best_proc.priority)
            self.log_gantt(self.running.pid)

    def peek_best_ready(self) -> Optional[Process]:
        for priority in range(MAX_PRIORITY + 1):
            queue = self.ready_queues[priority]
            if queue:
                return queue[0]
        return None

    def pop_ready(self, priority: int) -> Process:
        queue = self.ready_queues[priority]
        proc = queue.popleft()
        proc.state = "running"
        if proc.start_time is None:
            proc.start_time = self.time
        return proc

    def deadlock_detection(self) -> None:
        if not self.waiting_for_resources:
            return
        available = dict(self.rm.available)
        finish: Dict[int, bool] = {}
        wait_reqs: Dict[int, Tuple[int, int]] = {}
        for proc in self.waiting_for_resources:
            finish[proc.pid] = False
            if proc.pending_request:
                wait_reqs[proc.pid] = proc.pending_request
        for proc in self.processes.values():
            if proc.state not in {"waiting"}:
                finish[proc.pid] = True
        changed = True
        while changed:
            changed = False
            for pid, done in finish.items():
                if done:
                    continue
                req = wait_reqs.get(pid)
                if not req:
                    finish[pid] = True
                    changed = True
                    continue
                rid, amount = req
                if available.get(rid, 0) >= amount:
                    finish[pid] = True
                    allocated = self.rm.allocated.get(pid, {})
                    for arid, aamt in allocated.items():
                        available[arid] = available.get(arid, 0) + aamt
                    changed = True
        deadlocked = [pid for pid, done in finish.items() if not done]
        if deadlocked:
            victim = max(deadlocked, key=lambda p: self.processes[p].priority)
            proc = self.processes[victim]
            self.deadlock_log.append(
                f"t={self.time}: Deadlock detected, terminating process {victim}"
            )
            self.terminate_process(proc, due_to_deadlock=True)

    def terminate_process(self, proc: Process, due_to_deadlock: bool = False) -> None:
        proc.state = "terminated" if due_to_deadlock else "finished"
        proc.finish_time = self.time
        self.rm.release_all(proc.pid)
        if proc in self.waiting_for_resources:
            self.waiting_for_resources.remove(proc)
        if proc in self.io_queue:
            self.io_queue.remove(proc)
        self.completed += 1
        if self.running == proc:
            self.running = None

    def step_running(self) -> None:
        if not self.running:
            return
        proc = self.running
        burst = proc.current_burst()
        if not burst:
            self.terminate_process(proc)
            return
        if burst.kind != "CPU":
            raise RuntimeError("Running process without CPU burst")
        if proc.current_action_index >= len(burst.actions):
            self.finish_cpu_burst(proc)
            return
        action = burst.actions[proc.current_action_index]
        if action.kind == "run":
            if proc.action_remaining == 0:
                proc.action_remaining = action.value
            proc.action_remaining -= 1
            proc.quantum_remaining -= 1
            if proc.action_remaining == 0:
                proc.current_action_index += 1
        elif action.kind == "request":
            if proc.pending_request is None:
                proc.pending_request = (action.value, action.amount)
            rid, amount = proc.pending_request
            granted = self.rm.request(proc.pid, rid, amount)
            if granted:
                proc.pending_request = None
                proc.current_action_index += 1
            else:
                proc.state = "waiting"
                self.waiting_for_resources.append(proc)
                self.running = None
                self.log_gantt(None)
                return
        elif action.kind == "release":
            self.rm.release(proc.pid, action.value, action.amount)
            proc.current_action_index += 1

        if proc.current_action_index >= len(burst.actions):
            self.finish_cpu_burst(proc)
            return

        if proc.quantum_remaining == 0:
            proc.state = "ready"
            proc.reset_quantum()
            proc.ready_wait_counter = 0
            self.ready_queues[proc.priority].append(proc)
            self.running = None
            self.log_gantt(None)

    def finish_cpu_burst(self, proc: Process) -> None:
        proc.current_burst_index += 1
        proc.current_action_index = 0
        proc.action_remaining = 0
        if not proc.has_more_bursts():
            proc.finish_time = self.time
            proc.state = "finished"
            self.rm.release_all(proc.pid)
            self.completed += 1
            self.running = None
            self.log_gantt(None)
            return
        next_burst = proc.current_burst()
        if next_burst.kind == "IO":
            proc.io_remaining = next_burst.duration
            proc.current_burst_index += 1
            proc.state = "io"
            self.io_queue.append(proc)
            self.running = None
            self.log_gantt(None)
        else:
            proc.state = "ready"
            proc.reset_quantum()
            self.ready_queues[proc.priority].append(proc)
            self.running = None
            self.log_gantt(None)

    def process_io(self) -> None:
        finished = []
        for proc in self.io_queue:
            proc.io_remaining -= 1
            if proc.io_remaining <= 0:
                finished.append(proc)
        for proc in finished:
            self.io_queue.remove(proc)
            if not proc.has_more_bursts():
                proc.finish_time = self.time
                proc.state = "finished"
                self.rm.release_all(proc.pid)
                self.completed += 1
            else:
                proc.state = "ready"
                proc.reset_quantum()
                self.ready_queues[proc.priority].append(proc)

    def process_waiting_requests(self) -> None:
        to_ready = []
        for proc in list(self.waiting_for_resources):
            if proc.pending_request:
                rid, amt = proc.pending_request
                if self.rm.request(proc.pid, rid, amt):
                    proc.pending_request = None
                    to_ready.append(proc)
        for proc in to_ready:
            proc.state = "ready"
            proc.reset_quantum()
            proc.current_action_index += 1
            self.waiting_for_resources.remove(proc)
            self.ready_queues[proc.priority].append(proc)

    def update_wait_times(self) -> None:
        for queue in self.ready_queues.values():
            for proc in queue:
                proc.waiting_time += 1

    def add_arrivals(self) -> None:
        for proc in self.processes.values():
            if proc.state == "new" and proc.arrival <= self.time:
                self.add_to_ready(proc)

    def all_done(self) -> bool:
        return self.completed == self.total_processes

    def run(self) -> None:
        while not self.all_done():
            self.add_arrivals()
            self.process_waiting_requests()
            self.process_io()
            self.aging()
            self.schedule()
            if self.running is None and self.peek_best_ready():
                self.running = self.pick_next_process()
                self.log_gantt(self.running.pid if self.running else None)
            self.update_wait_times()
            if self.running:
                self.step_running()
            else:
                self.log_gantt(None)
            self.deadlock_detection()
            self.time += 1
        self.finalize_gantt()

    def report(self) -> None:
        print("\n=== Simulation Report ===")
        print("Gantt Chart:")
        for start, end, pid in self.gantt:
            label = f"P{pid}" if pid is not None else "IDLE"
            print(f"[{start:>4} - {end:>4}]: {label}")

        total_wait = 0
        total_turnaround = 0
        for proc in self.processes.values():
            finish = proc.finish_time or self.time
            turnaround = finish - proc.arrival
            total_turnaround += turnaround
            total_wait += proc.waiting_time

        n = len(self.processes)
        print(f"\nAverage Waiting Time: {total_wait / n:.2f}")
        print(f"Average Turnaround Time: {total_turnaround / n:.2f}")

        if self.deadlock_log:
            print("\nDeadlock Events:")
            for entry in self.deadlock_log:
                print(entry)
        else:
            print("\nNo deadlocks detected.")


def simulate(file_path: str) -> None:
    rm, processes = load_processes(file_path)
    scheduler = Scheduler(rm, processes)
    scheduler.run()
    scheduler.report()


def main() -> None:
    parser = argparse.ArgumentParser(description="OS Scheduling Simulator")
    parser.add_argument(
        "input_file",
        type=str,
        help="Path to the input scenario file describing resources and processes.",
    )
    args = parser.parse_args()
    simulate(args.input_file)


if __name__ == "__main__":
    main()
