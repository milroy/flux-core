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

import flux
from flux import util
from flux import debugged

class GetServerAction(argparse.Action):
    """Convenience class 
    """

    def __call__(self, parser, namespace, values, option_string=None):
        print("serveraction")


class KubeCmd:
    """
    KubeCmd is the base class for all flux-kube subcommands
    """

    def __init__(self, **kwargs):
        self.parser = self.create_parser(kwargs)

        k8s_client = config.new_client_from_config ()
        try:
            self.dyn_client = DynamicClient (k8s_client)
        except client.rest.ApiException as e:
            print ('You must be logged in to the K8s or OpenShift cluster to continue')
            sys.exit (1)

        v1_depl = self.dyn_client.resources.get (api_version='v1', kind='Deployment')
        depl_list = v1_depl.get (namespace='milroy1')
        for depl in depl_list.items:
            print(depl.metadata.name)

    @staticmethod
    def create_parser(exclude_io=False):
        """
        Create default parser with args for mini subcommands
        """
        parser = argparse.ArgumentParser(add_help=False)

        return parser

    def get_parser(self):
        return self.parser

class GetServerCmd(KubeCmd):
    """
    GetServerCmd gets K8s server info ...
    """

    def __init__(self):
        super().__init__()

        self.parser.add_argument(
            "--get-apiserver",
            type=str,
            action=GetServerAction,
            help="Get API server info",
        )

    def main(self, args):
        print('Got server info')


LOGGER = logging.getLogger("flux-kube")


@util.CLIMain(LOGGER)
def main():

    sys.stdout = open(
        sys.stdout.fileno(), "w", encoding="utf8", errors="surrogateescape"
    )
    sys.stderr = open(
        sys.stderr.fileno(), "w", encoding="utf8", errors="surrogateescape"
    )

    parser = argparse.ArgumentParser(prog="flux-kube")
    subparsers = parser.add_subparsers(
        title="supported subcommands", description="", dest="subcommand"
    )
    subparsers.required = True

    getserver = GetServerCmd()
    getserver_parser_sub = subparsers.add_parser(
        "getserver",
        parents=[getserver.get_parser()],
        help="get K8s API server",
        formatter_class=flux.util.help_formatter(),
    )
    getserver_parser_sub.set_defaults(func=getserver.main)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
