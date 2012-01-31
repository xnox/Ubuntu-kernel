#!/usr/bin/env python
#

# The code here is taken from http://martyalchin.com/2008/jan/10/simple-plugin-framework/
#

# So, what does it really take to implement a plugin "architecture"? Outside the framework,
# when implementing plugins themselves, there are three things that are really necessary:
#
# 1. A way to declare a mount point for plugins. Since plugins are an example of loose
#    coupling, there needs to be a neutral location, somewhere between the plugins and
#    the code that uses them, that each side of the system can look at, without having
#    to know the details of the other side. Trac calls this is an "extension point".
#
# 2. A way to register a plugin at a particular mount point. Since internal code can't
#    (or at the very least, shouldn't have to) look around to find plugins that might
#    work for it, there needs to be a way for plugins to announce their presence. This
#    allows the guts of the system to be blissfully ignorant of where the plugins come
#    from; again, it only needs to care about the mount point.
#
# 3. A way to retrieve the plugins that have been registered. Once the plugins have done
#    their thing at the mount point, the rest of the system needs to be able to iterate
#    over the installed plugins and use them according to its need.
#
# That may seem like an incredibly complicated task, and it certainly could be, as evidenced
# by the Zope implementation of the above requirements. But it can actually be done in just
# 6 lines of actual code. This may seem impossible, but the following code successfully
# fulfills all three of the above requirements.
#

class PluginMount(type):
    def __init__(cls, name, bases, attrs):
        if not hasattr(cls, 'plugins'):
            # This branch only executes when processing the mount point itself.
            # So, since this is a new plugin type, not an implementation, this
            # class shouldn't be registered as a plugin. Instead, it sets up a
            # list where plugins can be registered later.
            cls.plugins = []
        else:
            # This must be a plugin implementation, which should be registered.
            # Simply appending it to the list is all that's needed to keep track
            # of it later.
            cls.plugins.append(cls)

    def get_plugins(cls, *args, **kwargs):
        return [p(*args, **kwargs) for p in cls.plugins]


# vi:set ts=4 sw=4 expandtab:
