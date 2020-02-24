# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class Sos(CMakePackage):
    """SOS provides a flexible, scalable, and programmable framework for
    observation, introspection, feedback, and control of HPC applications."""

    homepage = "https://github.com/cdwdirect/sos_flow/wiki"
    url      = "https://github.com/cdwdirect/sos_flow.git"

    version('1.17',      git='https://github.com/cdwdirect/sos_flow.git', tag='spack-build-v1.17', preferred=True)
    version('chad_dev',  git='https://github.com/cdwdirect/sos_flow.git', branch='chad_dev')

    # Primary inter-daemon transport is EVPath for now.
    #depends_on('libevpath',        type=('build', 'run'))
    depends_on('sqlite@3:',        type=('build', 'run'))
    depends_on('python@2: +ucs4',  type=('build', 'run'))
    # Python modules used for CFFI build and common ML usage:
    depends_on('py-cffi',          type=('build', 'run'))
    #depends_on('py-dateutil',      type=('build', 'run'))
    #depends_on('py-numexpr',       type=('build', 'run'))
    #depends_on('py-numpy',         type=('build', 'run'))
    #depends_on('py-packaging',     type=('build', 'run'))
    #depends_on('py-pandas',        type=('build', 'run'))
    #depends_on('py-pycparser',     type=('build', 'run'))
    #depends_on('py-pyparsing',     type=('build', 'run'))
    #depends_on('py-pytz',          type=('build', 'run'))
    #depends_on('py-scikit-learn',  type=('build', 'run'))
    #depends_on('py-scipy',         type=('build', 'run'))
    #depends_on('py-six',           type=('build', 'run'))

    parallel = False

    def setup_environment(self, spack_env, run_env):
        #
        # Build settings:
        spack_env.set('SOS_DIR', self.spec.prefix)
        #
        # Runtime defaults:
        run_env.set('SOS_DIR', self.spec.prefix)
        run_env.set('SOS_ROOT', self.spec.prefix)
        run_env.set('SOS_BUILD_DIR', self.spec.prefix)
        run_env.set('SOS_CMD_PORT', '22500')
        run_env.set('SOS_WORK', env['HOME'])
        run_env.set('SOS_EVPATH_MEETUP', env['HOME'])
        run_env.set('SOS_DISCOVERY_DIR', env['HOME'])
        #
        run_env.set('SOS_DB_DISABLED', 'FALSE')
        run_env.set('SOS_UPDATE_LATEST_FRAME', 'TRUE')
        run_env.set('SOS_IN_MEMORY_DATABASE', 'FALSE')
        run_env.set('SOS_EXPORT_DB_AT_EXIT', 'FALSE')
        #
        run_env.set('SOS_OPTIONS_FILE', '')
        run_env.set('SOS_SYSTEM_MONITOR_ENABLED', 'FALSE')
        run_env.set('SOS_SYSTEM_MONITOR_FREQ_USEC', '0')
        run_env.set('SOS_SHUTDOWN', 'FALSE')

        run_env.set('PYTHONPATH', ("%s:%s/lib" % (env['PYTHONPATH'], self.spec.prefix)))

        run_env.set('SOS_ENV_SET', 'TRUE')

