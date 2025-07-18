# -*- coding: utf-8 -*-
#
# Chapel documentation build configuration file, created by
# sphinx-quickstart on Thu Jan 29 08:44:44 2015.
#
# This file is execfile()d with the current directory set to its
# containing dir.
#
# Note that not all possible configuration values are present in this
# autogenerated file.
#
# All configuration values have a default; values that are commented out
# serve to show the default.

import sys
import os
import shutil
import pathlib
import sphinx.environment
import sphinx.util.logging

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
sys.path.insert(0, os.path.abspath('../'))
sys.path.insert(0, os.path.abspath('./'))

# to prevent failures if chapel-py is not built/installed, check if its installed
# if installed, add the path and generate the rst file
# if not installed, just create the file so that the build doesn't fail
old_sys_path = sys.path.copy()
sys.path.insert(0, os.path.abspath('../../util/chplenv'))
import chpl_home_utils
chapel_py_dir = chpl_home_utils.get_chpldeps(chapel_py=True)
del chpl_home_utils
sys.path = old_sys_path

include_chapel_py_docs = False
chapel_py_api_template = os.path.abspath("./tools/chapel-py/chapel-py-api-template.rst")
chapel_py_api_rst = os.path.abspath("./tools/chapel-py/chapel-py-api.rst")
if os.path.exists(chapel_py_dir):
    sys.path.insert(0, chapel_py_dir)
    include_chapel_py_docs = True
    shutil.copyfile(chapel_py_api_template, chapel_py_api_rst)
else:
    if os.path.exists(chapel_py_api_rst):
        os.remove(chapel_py_api_rst)
    pathlib.Path(chapel_py_api_rst).touch()
    msg = (
        "chapel-py not built, skipping API docs generation. "
        "Run `make chapel-py-venv` to build chapel-py."
    )
    sphinx.util.logging.getLogger(__name__).info(msg, color="red")

# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
needs_sphinx = '1.3'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.todo',
    'sphinxcontrib.jquery',
    'sphinxcontrib.chapeldomain',
    'sphinx.ext.mathjax',
    'util.disguise',
    'breathe',
    'search_index_entries',
]

breathe_default_project = "dyno"
# don't generate #include suggestions in docs because
# it tries to link to the source code which we're not setup for
breathe_show_include = False

nitpick_ignore_regex = [('cpp:identifier', r'llvm(:.*)?')]
nitpick_ignore = []
for line in open('../util/nitpick_ignore'):
    if line.strip() == "" or line.startswith("#"):
        continue
    dtype, target = line.split(None, 1)
    target = target.strip()
    nitpick_ignore.append((dtype, target))

# Add any paths that contain templates here, relative to this directory.
templates_path = ['meta/templates']


# sphinx can't handle extension modules and pyi, so we have to do it ourselves
# https://github.com/sphinx-doc/sphinx/issues/7630
# load in the pyi file
from process_pyi import PyiSignatures
if include_chapel_py_docs:
    chapel_pyi = PyiSignatures(chapel_py_dir + '/chapel/core.pyi')
# autodoc-process-signature
def process_signature(app, what, name, obj, options, signature, return_annotation):
    if what == 'method':
        _, class_name, method_name = name.rsplit('.', 2)
        res = chapel_pyi.get(class_name, method_name)
        if res:
            signature, return_annotation = res
    return signature, return_annotation

# Setup CSS files
def setup(app):
    app.add_css_file('style.css')

    if include_chapel_py_docs:
        app.connect('autodoc-process-signature', process_signature)

# The suffix of source filenames.
source_suffix = '.rst'

# The encoding of source files.
# source_encoding = 'utf-8-sig'

# The master toctree document.
master_doc = 'index'

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.
#
# The short X.Y version.

# We use a custom version variable (shortversion) instead, because setting
# 'version' adds a redundant version number onto the top of the sidebar
# automatically (rtd-theme). We also don't use |version| anywhere in rst

chplversion = '2.6'
shortversion = chplversion.replace('-', '&#8209') # prevent line-break at hyphen, if any
html_context = {"chplversion":chplversion}

# The full version, including alpha/beta/rc tags.
release = '2.6.0 (pre-release)'

# General information about the project.
project = u'Chapel Documentation'

author_text = os.environ.get('CHPLDOC_AUTHOR', '')

copyright_year = 2025
copyright = u'{0}, {1}'.format(copyright_year, author_text)


# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
# language = None

# There are two options for replacing |today|: either, you set today to some
# non-false value, then it is used:
# today = ''
# Else, today_fmt is used as the format for a strftime call.
# today_fmt = '%B %d, %Y'

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
exclude_patterns = ['Makefile',
                    'Makefile.sphinx',
                    'developer/chips',
                    'developer/implementation',
                    'util',
                    'meta',
                    'usingchapel/editors',

                    # These don't need to be processed separately
                    # since they are included in the spec with .. include::
                    'builtins/Atomics.rst',
                    'builtins/Bytes.rst',
                    'builtins/ChapelArray.rst',
                    'builtins/ChapelDomain.rst',
                    'builtins/ChapelLocale.rst',
                    'builtins/ChapelRange.rst',
                    'builtins/ChapelSyncvar.rst',
                    'builtins/ChapelTuple.rst',
                    'builtins/OwnedObject.rst',
                    'builtins/SharedObject.rst',
                    'builtins/String.rst',
                    'modules/standard/AutoMath.rst',
                    'modules/standard/AutoGpu.rst',
                    'modules/standard/ChapelIO.rst',
                    'modules/standard/ChapelSysCTypes.rst',

                    # exclude the chapel-py files
                    'tools/chapel-py/chapel-py-api-template.rst',
                    'tools/chapel-py/chapel-py-api.rst',
                    'tools/chplcheck/generated/rules.rst',
                   ]

# The reST default role (used for this markup: `text`) to use for all
# documents.
# default_role = None

# If true, '()' will be appended to :func: etc. cross-reference text.
# add_function_parentheses = True

# If true, the current module name will be prepended to all description
# unit titles (such as .. function::).
# add_module_names = True

# If true, sectionauthor and moduleauthor directives will be shown in the
# output. They are ignored by default.
# show_authors = False

# The name of the Pygments (syntax highlighting) style to use.
# pygments_style = 'sphinx'

# A list of ignored prefixes for module index sorting.
# modindex_common_prefix = []

# If true, keep warnings as "system message" paragraphs in the built documents.
# keep_warnings = False


# -- Options for HTML output ----------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
if not on_rtd:
    import sphinx_rtd_theme
    html_theme = 'sphinx_rtd_theme'
    html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]

    html_theme_options = {
        'sticky_navigation': True,
    }



# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
# html_theme_options = {}

# Add any paths that contain custom themes here, relative to this directory.
# html_theme_path = []

# The name for this set of Sphinx documents.  If None, it defaults to
# "<project> v<release> documentation".
# We set this because the default title is repetitive
html_title = 'Chapel Documentation {0}'.format(chplversion)

# A shorter title for the navigation bar.  Default is the same as html_title.
# html_short_title = None

# The name of an image file (relative to this directory) to place at the top
# of the sidebar.
# html_logo = None

# The name of an image file (within the static path) to use as favicon of the
# docs.  This file should be a Windows icon file (.ico) being 16x16 or 32x32
# pixels large.
html_favicon = 'meta/static/favicon.ico'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['meta/static']

# Add any extra paths that contain custom files (such as robots.txt or
# .htaccess) here, relative to this directory. These files are copied
# directly to the root of the documentation.
# html_extra_path = []

# If not '', a 'Last updated on:' timestamp is inserted at every page bottom,
# using the given strftime format.
# html_last_updated_fmt = '%b %d, %Y'

# Custom sidebar templates, maps document names to template names.
# html_sidebars = {}

# Additional templates that should be rendered to pages, maps page names to
# template names.
# html_additional_pages = {}

# If false, no module index is generated.
# html_domain_indices = True

# If false, no index is generated.
# html_use_index = True

# If true, the index is split into individual pages for each letter.
# html_split_index = False

# If true, links to the reST sources are added to the pages.
# html_show_sourcelink = True

# If true, "Created using Sphinx" is shown in the HTML footer. Default is True.
html_show_sphinx = False

# If true, "(C) Copyright ..." is shown in the HTML footer. Default is True.
# html_show_copyright = True

# If true, an OpenSearch description file will be output, and all pages will
# contain a <link> tag referring to it.  The value of this option must be the
# base URL from which the finished HTML is served.
# html_use_opensearch = ''

# This is the file name suffix for HTML files (e.g. ".xhtml").
# html_file_suffix = None

# Output file base name for HTML help builder.
htmlhelp_basename = 'chapel'


# -- Options for LaTeX output ---------------------------------------------

latex_elements = {
# The paper size ('letterpaper' or 'a4paper').
#'papersize': 'letterpaper',

# The font size ('10pt', '11pt' or '12pt').
#'pointsize': '10pt',

# Additional stuff for the LaTeX preamble.
#'preamble': '',
}

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title,
#  author, documentclass [howto, manual, or own class]).
latex_documents = [
  ('index', 'chapel.tex', u'Chapel Documentation',
   author_text, 'manual'),
]

# The name of an image file (relative to this directory) to place at the top of
# the title page.
# latex_logo = None

# For "manual" documents, if this is true, then toplevel headings are parts,
# not chapters.
# latex_use_parts = False

# If true, show page references after internal links.
# latex_show_pagerefs = False

# If true, show URL addresses after external links.
# latex_show_urls = False

# Documents to append as an appendix to all manuals.
# latex_appendices = []

# If false, no module index is generated.
# latex_domain_indices = True


# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    ('index', 'chapel', u'Chapel Documentation',
     [author_text], 1)
]

# If true, show URL addresses after external links.
# man_show_urls = False


# -- Options for Texinfo output -------------------------------------------

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
  ('index', 'chapel', u'Chapel Documentation',
   author_text, 'chapel',
   'Chapel is an emerging programming language designed for productive parallel computing at scale.',
   'Miscellaneous'),
]

# Documents to append as an appendix to all manuals.
# texinfo_appendices = []

# If false, no module index is generated.
# texinfo_domain_indices = True

# How to display URL addresses: 'footnote', 'no', or 'inline'.
# texinfo_show_urls = 'footnote'

# If true, do not generate a @detailmenu in the "Top" node's menu.
# texinfo_no_detailmenu = False

# -- Custom options -------------------------------------------------------

### Custom lexers for syntax listings
from pygments.lexer import RegexLexer
from pygments import token
from sphinx.highlighting import lexers

class TrivialLexer(RegexLexer):
    name = 'trivial'

    tokens = {
        'root': [
            (r'.*\n', token.Text)
            #(r'MyKeyword', token.Keyword),
            #(r'[a-zA-Z]', token.Name),
            #(r'\s', token.Text)
        ]
    }

lexers['syntax'] = TrivialLexer(startinline=True)
lexers['syntaxdonotcollect'] = TrivialLexer(startinline=True)
lexers['printoutput'] = TrivialLexer(startinline=True)
