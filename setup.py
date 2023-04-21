from distutils.core import setup, Extension

setup(
	name = 'YouTube WebVTT Parser',
	version = '1.0',
	description = 'A module to read and clean automatically generated subtitles'
	'by YouTube.',
	ext_modules = [
		Extension('yt_webvtt', sources = ['yt_webvtt.c'], py_limited_api = True)
	]
)
