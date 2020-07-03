#!/usr/bin/env bash

set -e

get_value(){
    grep -Po "(?<=^${1}=).*" ${2}
}

get_value "$@"
