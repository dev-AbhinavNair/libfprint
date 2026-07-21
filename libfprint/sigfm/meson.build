
sigfm_sources = ['sigfm.cpp']

opencv = dependency('opencv5', required: true, version: '>=5.0.0')

libsigfm = static_library('sigfm',
        sigfm_sources,
        dependencies: [opencv],
)
