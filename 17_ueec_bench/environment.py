"""Best-effort, read-only environment inventory; never infer host DIMM details.

Guest-visible resources and cgroup limits are recorded separately. Detection
tools can be absent or hidden; their output is evidence, not proof of bare metal.
"""
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path


def read_text(path):
    try:
        return Path(path).read_text().strip()
    except (OSError, UnicodeError):
        return None


def command(argv):
    if not shutil.which(argv[0]):
        return {"status": "unavailable", "stdout": None}
    try:
        p = subprocess.run(argv, capture_output=True, text=True, timeout=5,
                           env={**os.environ, "LC_ALL": "C"})
        return {"status": "ok" if p.returncode == 0 else "nonzero",
                "exit_code": p.returncode, "stdout": p.stdout.strip() or None}
    except (OSError, subprocess.TimeoutExpired) as e:
        return {"status": type(e).__name__, "stdout": None}


def memory_snapshot():
    raw = read_text('/proc/meminfo') or ''
    values = {}
    for line in raw.splitlines():
        key, _, value = line.partition(':')
        fields = value.split()
        if fields and fields[0].isdigit():
            values[key] = int(fields[0]) * (1024 if len(fields) > 1 and fields[1] == 'kB' else 1)
    return {key + '_bytes': values.get(key) for key in
            ('MemTotal', 'MemAvailable', 'SwapTotal', 'SwapFree')}


def cgroup_inventory():
    # Resolve the process's actual cgroup, not an unrelated root-group limit.
    # v1/hybrid and nonstandard mount layouts are explicitly unsupported here.
    raw = read_text('/proc/self/cgroup') or ''
    candidates = []
    for line in raw.splitlines():
        if line.startswith('0::'):
            rel = line[3:].lstrip('/')
            if '..' not in Path(rel).parts:
                candidates.append(Path('/sys/fs/cgroup') / rel)
    root = Path('/sys/fs/cgroup')
    # A cgroup namespace can expose its own group at the mount root.
    if (root / 'cgroup.controllers').exists():
        candidates.append(root)
    group = next((p for p in candidates if (p / 'memory.max').exists()), None)
    if group is None:
        return {"version": None, "status": "unavailable or unsupported layout"}
    keys = ('memory.max', 'memory.high', 'memory.current', 'memory.swap.max',
            'memory.swap.current', 'cpu.max', 'cpuset.cpus.effective')
    limits = {key: read_text(group / key) for key in keys}
    # Parent constraints may be tighter than the leaf. Retain visible ancestry.
    ancestors = []
    for parent in group.parents:
        if parent == root.parent or root not in (parent, *parent.parents):
            break
        ancestors.append({key: read_text(parent / key) for key in keys
                          if key not in ('memory.current', 'memory.swap.current')})
    return {"version": 2, "visible_group": limits, "visible_ancestor_limits": ancestors,
            "note": "Host or namespace-hidden ancestor constraints may be unavailable."}


def observe():
    return {"memory": memory_snapshot(), "cgroup": cgroup_inventory(),
            "load_average_1_5_15_minutes": list(os.getloadavg()) if hasattr(os, 'getloadavg') else None}


def inventory(affinity, note=None):
    lscpu = command(['lscpu', '--json'])
    if lscpu['stdout']:
        try:
            lscpu['data'] = json.loads(lscpu.pop('stdout'))
        except (ValueError, TypeError):
            lscpu['status'] = 'unparseable output'
    cpuinfo = read_text('/proc/cpuinfo') or ''
    first = cpuinfo.split('\n\n')[0]
    cpu = {}
    for line in first.splitlines():
        key, sep, value = line.partition(':')
        if sep:
            cpu[key.strip()] = value.strip()
    representative = affinity[0] if affinity else 0
    cpudir = Path(f'/sys/devices/system/cpu/cpu{representative}')
    frequency = {name: read_text(cpudir / 'cpufreq' / name) for name in
                 ('scaling_driver', 'scaling_governor', 'cpuinfo_min_freq',
                  'cpuinfo_max_freq', 'scaling_min_freq', 'scaling_max_freq',
                  'scaling_cur_freq')}
    caches = []
    for index in sorted((cpudir / 'cache').glob('index*')):
        caches.append({key: read_text(index / key) for key in
                       ('level', 'type', 'size', 'coherency_line_size', 'shared_cpu_list')})
    detect = {kind: command(['systemd-detect-virt', '--' + kind])
              for kind in ('vm', 'container')}
    dmi = {key: read_text(Path('/sys/class/dmi/id') / key) for key in
           ('sys_vendor', 'product_name', 'product_version')}
    return {
        'os': {'system': platform.system(), 'kernel': platform.release(),
               'machine': platform.machine(), 'distribution': platform.freedesktop_os_release()
               if Path('/etc/os-release').exists() else None,
               'python': platform.python_version()},
        'cpu': {'visible_logical_cpus': os.cpu_count(), 'allowed_cpu_affinity': affinity,
                'first_visible_processor': cpu, 'lscpu': lscpu,
                'frequency_snapshot_khz': frequency, 'cache_for_representative_cpu': caches,
                'representative_cpu': representative,
                'frequency_note': 'Snapshot/limits only; clock is not fixed by this benchmark.'},
        'memory': {**memory_snapshot(), 'page_size_bytes': os.sysconf('SC_PAGE_SIZE'),
                   'physical_DIMM_type': None, 'physical_DIMM_speed_MT_s': None,
                   'note': 'Guest/OS-visible RAM and swap; physical DIMM parameters are not inferred.'},
        'virtualization': {'detectors': detect, 'dmi_product_evidence': dmi,
                           'hypervisor_version': None,
                           'container_markers': {p: Path(p).exists() for p in
                                                 ('/.dockerenv', '/run/.containerenv')},
                           'note': 'Record observed VM/container identifiers. Missing evidence does not prove bare metal; nested host details may be hidden.'},
        'cgroup': cgroup_inventory(), 'user_supplied_environment_note': note,
    }
