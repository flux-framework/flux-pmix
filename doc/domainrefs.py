"""
Originally from:

  https://github.com/mitogen-hq/mitogen/blob/master/docs/domainrefs.py

Copyright 2021, the Mitogen authors

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
import functools
import re

import docutils.nodes
import docutils.utils
from sphinx.domains import Domain


CUSTOM_RE = re.compile("(.*) <(.*)>")


def role(config, role, rawtext, text, lineno, inliner, options={}, content=[]):
    template = "https://docs.ansible.com/ansible/latest/modules/%s_module.html"

    match = CUSTOM_RE.match(text)
    if match:  # "custom text <real link>"
        title = match.group(1)
        text = match.group(2)
    elif text.startswith("~"):  # brief
        text = text[1:]
        title = config.get("brief", "%s") % (docutils.utils.unescape(text),)
    else:
        title = config.get("text", "%s") % (docutils.utils.unescape(text),)

    node = docutils.nodes.reference(
        rawsource=rawtext, text=title, refuri=config["url"] % (text,), **options
    )

    return [node], []


def setup(app):
    # Sphinx reads "prefix:name" as a domain lookup, and warns about an
    # unknown domain before it falls back to the global role of that
    # name, so a prefixed role needs a domain to be found quietly.
    # Register one per prefix, holding the roles that use it.
    domains = {}
    for name, info in app.config._raw_config["domainrefs"].items():
        func = functools.partial(role, info)
        if ":" in name:
            prefix, suffix = name.split(":", 1)
            domains.setdefault(prefix, {})[suffix] = func
        else:
            app.add_role(name, func)
    for prefix, roles in domains.items():
        app.add_domain(
            type(
                f"{prefix.capitalize()}RefsDomain",
                (Domain,),
                {"name": prefix, "label": prefix, "roles": roles},
            )
        )
