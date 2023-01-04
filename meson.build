project(
	'waywalker',
	'c',
	version: '0.0.1',
	license: 'MIT',
	meson_version: '>=0.58.1',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')

cc = meson.get_compiler('c')
# Clang complains about some zeroed initializer lists (= {0}), even though they
# are valid
if cc.get_id() == 'clang'
	add_project_arguments('-Wno-missing-field-initializers', language: 'c')
	add_project_arguments('-Wno-missing-braces', language: 'c')
endif

# Avoid wl_buffer deprecation warnings
add_project_arguments('-DWL_HIDE_DEPRECATED', language: 'c')
add_project_arguments('-DWLR_USE_UNSTABLE', language: 'c')

pixman         = dependency('pixman-1')
wlroots        = dependency('wlroots')
wayland_server = dependency('wayland-server')
xkbcommon      = dependency('xkbcommon')

#### Generate xdg-shell protocol ####
wlr_files = []
wayland_protos = dependency('wayland-protocols')
wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_variable('wayland_scanner'),
	native: true,
)

protocol_name = 'xdg-shell'
protocol_path = wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml'

wlr_files = []
code = custom_target(
	protocol_name.underscorify() + '_c',
	input: protocol_path,
	output: '@BASENAME@-protocol.c',
	command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)
wlr_files += code

server_header = custom_target(
	protocol_name.underscorify() + '_server_h',
	input: protocol_path,
	output: '@BASENAME@-protocol.h',
	command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)
wlr_files += server_header

client_header = custom_target(
	protocol_name.underscorify() + '_client_h',
	input: protocol_path,
	output: '@BASENAME@-client-protocol.h',
	command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
	build_by_default: false,
)

lib_xdg_shell = library(
	'lib_xdg_shell', wlr_files,
	dependencies: wayland_server,
	install: true

)
#### end ####

executable(
	'waywalker',
	[
		'src/waywalker.c'
	],
	dependencies: [
		pixman,
		wlroots,
		wayland_server,
		xkbcommon,
	],
	link_with: lib_xdg_shell
)
