#!/bin/bash
set -e

uv sync
uv run pre-commit install
