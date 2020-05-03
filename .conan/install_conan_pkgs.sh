#!/usr/bin/env sh

cd "$(dirname "$0")"

for d in ./*_pkg ; do
    conan create "${d}/conanfile.py"
done
