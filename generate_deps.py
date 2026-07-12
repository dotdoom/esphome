#!/usr/bin/env python3
import sys
import re
from pathlib import Path


def get_deps(file_path, visited=None):
    if visited is None:
        visited = set()
    if str(file_path) in visited:
        return set()
    visited.add(str(file_path))

    deps = set()
    p = Path(file_path)
    if not p.is_file():
        return deps

    with open(p, "r", encoding="utf-8") as f:
        content = f.read()

    matches = re.findall(r"!include\s+([a-zA-Z0-9_/-]+\.yaml)", content)
    for match in matches:
        # Resolve path relative to the file being processed
        if match.startswith("/"):
            dep_path = Path(match)
        else:
            dep_path = (p.parent / match).resolve().relative_to(Path.cwd())

        deps.add(str(dep_path))
        deps.update(get_deps(dep_path, visited))

    # Also track local external_components directories
    source_matches = re.findall(r"source:\s+([a-zA-Z0-9_/-]+)", content)
    for match in source_matches:
        if match.startswith("/"):
            source_dir = Path(match)
        else:
            source_dir = (p.parent / match).resolve().relative_to(Path.cwd())

        if source_dir.is_dir():
            for f in source_dir.rglob("*"):
                if f.is_file():
                    deps.add(str(f))

    return deps


def main():
    devices = sys.argv[1].split()
    btproxy_replicas = sys.argv[2].split()

    for device in devices:
        yaml_file = f"{device}.yaml"
        if not Path(yaml_file).is_file():
            continue

        deps = get_deps(yaml_file)
        if deps:
            deps_str = " ".join(sorted(deps))
            print(f"$(STAMP_DIR)/.stamp-compile-{device}: {deps_str}")
            print(f"$(STAMP_DIR)/.stamp-flash-{device}: {deps_str}")

    if Path("btproxy.yaml").is_file():
        deps = get_deps("btproxy.yaml")
        if deps:
            deps_str = " ".join(sorted(deps))
            for replica in btproxy_replicas:
                print(f"$(STAMP_DIR)/.stamp-compile-btproxy-{replica}: {deps_str}")
                print(f"$(STAMP_DIR)/.stamp-flash-btproxy-{replica}: {deps_str}")


if __name__ == "__main__":
    main()
