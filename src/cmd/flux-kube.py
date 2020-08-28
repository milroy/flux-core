##############################################################
# Copyright 2019 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

import sys
try:
    from kubernetes import client, config
except ImportError:
    print ('Error: K8s Python client not installed')
    sys.exit (1)

try:
    from openshift.dynamic import DynamicClient                  
except ImportError:
    print ('Error: OpenShift Python client not installed')
    sys.exit (1)

import os
import logging
import argparse
import json
import yaml
import re
from itertools import chain

import flux

print ('testing')
