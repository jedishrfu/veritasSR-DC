#!/bin/zsh
set -eu
project_root="${0:A:h:h:h}"
sh "$project_root/scripts/build-experiment.sh" "${SR_BUILD_PRESET:-release}" \
    ex0_pwpr ex0_cheb_pwpr ex0_readfloats ex0_spdp
print "Executables: $project_root/build/${SR_BUILD_PRESET:-release}/bin"
