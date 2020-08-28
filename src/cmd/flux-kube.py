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
    import openshift as oc
except ImportError as e:
    print (e)
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

k8s_client = config.new_client_from_config ()
try:
    dyn_client = DynamicClient (k8s_client)
except client.rest.ApiException as e:
    print ('You must be logged in to the K8s or OpenShift cluster to continue')
    sys.exit (1)

v1_depl = dyn_client.resources.get (api_version='v1', kind='Deployment')
depl_list = v1_depl.get (namespace='milroy1')

for depl in depl_list.items:
    print (depl.metadata.name)
