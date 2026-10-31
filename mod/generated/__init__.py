# Tabula Rasa mouselook fix: entry point.
#
# The mod folder is a [Data] path in tabula_rasa.cfg, named first. The retail
# clientbootstrap.boot() puts every [Data] path on sys.path, so the client's first
# 'import generated...' (the game tables) runs this file instead of the empty retail one.
# This file:
#   1. imports the native module trmousefix.pyd (in the mod folder), which fixes the mouse,
#      and gives it the game's UI size once the UI exists;
#   2. adds the game.zip package folder to __path__, so all generated.* modules still come
#      from game.zip;
#   3. runs the generated/__init__.py of the next mod folder on sys.path, if there is one
#      (for example the Infinite Rasa client mod), so that mod still starts.
#
# Python 2.4. Nothing here may stop the game from starting: every step is guarded.

import sys
import os
import time

try:
    _log_path = os.path.join(os.path.dirname(sys.executable or ''), 'trmousefix.log')
except:
    _log_path = 'trmousefix.log'


def _log(message):
    try:
        f = open(_log_path, 'a')
        try:
            f.write('%s %s\n' % (time.strftime('%Y-%m-%d %H:%M:%S'), message))
        finally:
            f.close()
    except:
        pass


_own = os.path.normcase(os.path.abspath(os.path.dirname(os.path.dirname(__file__))))

# 1. The native module.
try:
    import trmousefix
    _log('mod: native module loaded from %s' % (_own,))
except:
    _log('mod: native module not loaded: %s' % (sys.exc_info()[1],))

# 1b. The game's root window size, for the native module. The client makes its UI after this
# file runs, so a wrapper of __import__ waits for client.gameui; then a periodic update (the
# game calls it with the milliseconds since the last call) reads the root window size about
# once a second and gives it to the native module. A window or resolution change also changes
# the root window size, and the next update gives the new size. If the updates stop for more
# than 5 seconds (the game may make its list of updates again), the next import registers
# the update again.
_tr_state = {'last': 0.0, 'logged': False, 'since': 1000, 'size': None}


def _tr_update(msec=0):
    _tr_state['last'] = time.time()
    try:
        _tr_state['since'] = _tr_state['since'] + msec
        if _tr_state['since'] < 1000:
            return
        _tr_state['since'] = 0
        # The engine object: not a module, the engine puts it into the game modules' namespace.
        engine = getattr(sys.modules.get('client.gameui'), 'gameclient', None)
        if engine is None:
            return
        w, h = engine.GetUIManager().GetRootWindow().GetSize()
        size = (int(w), int(h))
        if size != _tr_state['size']:
            _tr_state['size'] = size
            trmousefix.set_virtual(size[0], size[1])
    except:
        pass


def _tr_register():
    # Register if client.gameui exists and does not have the update in its list (the module
    # may still be loading when it is first seen).
    gameui = sys.modules.get('client.gameui')
    if gameui is None or not hasattr(gameui, 'RegisterForUpdate'):
        return
    callables = getattr(gameui, 'g_updateCallables', None)
    if callables is not None and _tr_update in callables:
        return
    gameui.RegisterForUpdate(_tr_update)
    if not _tr_state['logged']:
        _tr_state['logged'] = True
        _log('mod: UI size updates registered')


try:
    import __builtin__
    if 'trmousefix' in sys.modules and hasattr(trmousefix, 'set_virtual'):
        _tr_real_import = __builtin__.__import__

        def _tr_import(*args, **kwargs):
            module = _tr_real_import(*args, **kwargs)
            if time.time() - _tr_state['last'] > 5.0:
                try:
                    _tr_register()
                except:
                    pass
            return module

        # Kept in place for the whole game (another mod may wrap it later); while the updates
        # arrive it only passes the call on.
        __builtin__.__import__ = _tr_import
except:
    _log('mod: import wrapper not set: %s' % (sys.exc_info()[1],))

# 2. The game tables from game.zip.
try:
    for _path in sys.path:
        if os.path.basename(_path).lower() == 'game.zip':
            _tables = _path + '/generated'
            if _tables not in __path__:
                __path__.append(_tables)
            break
    else:
        _log('mod: game.zip is not on sys.path %r' % (sys.path,))
except:
    _log('mod: game tables not added: %s' % (sys.exc_info()[1],))

# 3. The next mod folder's entry point (another mod that starts the same way). It runs as if it
# were the package's own __init__.py: its generated folder first on __path__, its own __file__.
if not getattr(sys, '_trmousefix_chained', False):
    sys._trmousefix_chained = True
    try:
        _after = False
        for _path in list(sys.path):
            _here = os.path.normcase(os.path.abspath(_path))
            if _here == _own:
                _after = True
                continue
            _init = os.path.join(_path, 'generated', '__init__.py')
            if _after and os.path.isfile(_init):
                _log('mod: starting the next mod folder %s' % (_path,))
                if os.path.dirname(_init) not in __path__:
                    __path__.insert(0, os.path.dirname(_init))
                _g = dict(globals())
                _g['__file__'] = _init
                execfile(_init, _g)
                # The chained file may change the package path: keep that.
                if '__path__' in _g:
                    globals()['__path__'] = _g['__path__']
                break
    except:
        _log('mod: the next mod folder failed: %s' % (sys.exc_info()[1],))
