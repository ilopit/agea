#!/usr/bin/env python3
"""Start development environment: VS Code, Visual Studio with kryga project, and Claude."""

import subprocess
import os
from pathlib import Path

ROOT_DIR = Path(__file__).parent.resolve()
VS_SOLUTION = ROOT_DIR / "build" / "project_Debug" / "kryga.sln"


def main():
    # Start VS Code in workspace directory
    print("Starting VS Code...")
    subprocess.Popen(["code", str(ROOT_DIR)], shell=True)

    # Start Visual Studio with kryga solution
    if VS_SOLUTION.exists():
        print("Starting Visual Studio with kryga.sln...")
        subprocess.Popen(["start", "", str(VS_SOLUTION)], shell=True)
    else:
        print(f"Warning: Solution not found at {VS_SOLUTION}")
        print("Run tools/configure.bat first to generate the solution.")

    # Start Claude Code CLI
    print("Starting Claude...")
    subprocess.Popen(["claude"], shell=True, cwd=str(ROOT_DIR))


if __name__ == "__main__":
    main()
