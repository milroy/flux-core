##############################################################
# Copyright 2020 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

import sys
import os
import logging
import argparse

import flux
from flux import util
from kubernetes import client, config
from openshift.dynamic import DynamicClient

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
            if e.status == 403:
                raise Exception("You must be logged in to the K8s or OpenShift"
                                   " cluster to continue")
            else:
                raise

    @staticmethod
    def create_parser(exclude_io=False):
        """
        Create default parser with args for mini subcommands
        """
        parser = argparse.ArgumentParser(add_help=False)

        return parser

    def get_parser(self):
        return self.parser

class GetDeploymentsCmd(KubeCmd):
    """
    GetServerCmd gets K8s server info ...
    """

    def __init__(self):
        super().__init__()

        self.parser.add_argument(
            "-n",
            "--namespace",
            type=str,
            metavar="N",
            help="Namespace of deployment",
        )

    def main(self, args):
        v1_depl = self.dyn_client.resources.get(api_version="v1", 
                                                kind="Deployment")
        try:
            depl_list = v1_depl.get(namespace=args.namespace)
        except client.rest.ApiException as e:
            raise

        if depl_list.items:
            for depl in depl_list.items:
                print("Deployment name:", depl.metadata.name)
                for item in depl:
                    print(item)
        else:
            print("No deployments in namespace", args.namespace)

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

    getdeployments = GetDeploymentsCmd()
    getdeployments_parser_sub = subparsers.add_parser(
        "getdeployments",
        parents=[getdeployments.get_parser()],
        help="get K8s deployment details",
        formatter_class=flux.util.help_formatter(),
    )
    getdeployments_parser_sub.set_defaults(func=getdeployments.main)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
