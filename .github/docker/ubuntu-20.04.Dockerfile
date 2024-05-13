# Copyright (C) 2023-2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#
# Dockerfile - a 'recipe' for Docker to build an image of ubuntu-based
#              environment for building the Unified Runtime project.
#

# Pull base image ("20.04")
FROM registry.hub.docker.com/library/ubuntu@sha256:f2034e7195f61334e6caff6ecf2e965f92d11e888309065da85ff50c617732b8

# Set environment variables
ENV OS ubuntu
ENV OS_VER 20.04
ENV NOTTY 1
ENV DEBIAN_FRONTEND noninteractive

# Additional parameters to build docker without building components.
# These ARGs can be set in docker building phase and are used
# within bash scripts (executed within docker).
ARG SKIP_DPCPP_BUILD
ARG SKIP_LIBBACKTRACE_BUILD

# Base development packages
ARG BASE_DEPS="\
	build-essential \
	cmake \
	git"

# Unified Runtime's dependencies
ARG UR_DEPS="\
	doxygen \
	python3 \
	python3-pip"

# Miscellaneous for our builds/CI (optional)
ARG MISC_DEPS="\
	clang \
	g++-7 \
	libncurses5 \
	sudo \
	wget \
	whois"

# Update and install required packages
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
	${BASE_DEPS} \
	${UR_DEPS} \
	${MISC_DEPS} \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get clean all

# Prepare a dir (accessible by anyone)
RUN mkdir --mode 777 /opt/ur/

# Additional dev. dependencies (installed via pip)
COPY third_party/requirements.txt /opt/ur/requirements.txt
RUN pip3 install --no-cache-dir -r /opt/ur/requirements.txt

# Install libbacktrace
COPY .github/docker/install_libbacktrace.sh /opt/ur/install_libbacktrace.sh
RUN /opt/ur/install_libbacktrace.sh

# Add a new (non-root) 'test_user'
ENV USER test_user
ENV USERPASS pass
RUN useradd -m "${USER}" -g sudo -p "$(mkpasswd ${USERPASS})"
