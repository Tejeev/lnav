
.. _hotkeys:

Hotkey Reference
================

This reference covers the keys used to control **lnav**.  Consult the `built-in
help <https://github.com/tstack/lnav/blob/master/src/help.txt>`_ in **lnav** for
a more detailed explanation of each key.

Spatial Navigation
------------------

.. list-table::
   :header-rows: 1
   :widths: 6 6 6 20

   * - Keypress
     -
     -
     - Command
   * - :kbd:`Space`
     - :kbd:`PgDn`
     -
     - Down a page
   * - :kbd:`b`
     - :kbd:`Backspace`
     - :kbd:`PgUp`
     - Up a page
   * - :kbd:`j`
     - :kbd:`Return`
     - :kbd:`↓`
     - Down a line
   * - :kbd:`k`
     - :kbd:`↑`
     -
     - Up a line
   * - :kbd:`h`
     - :kbd:`←`
     -
     - Left half a page.  In the log view, pressing left while at the start of
       the message text will reveal the source file name for each line.
       Pressing again will reveal the full path.
   * - :kbd:`Shift` + :kbd:`h`
     - :kbd:`Shift` + :kbd:`←`
     -
     - Left ten columns
   * - :kbd:`l`
     - :kbd:`→`
     -
     - Right half a page
   * - :kbd:`Shift` + :kbd:`l`
     - :kbd:`Shift` + :kbd:`→`
     -
     - Right ten columns
   * - :kbd:`Home`
     - :kbd:`g`
     -
     - Top of the view
   * - :kbd:`End`
     - :kbd:`G`
     -
     - Bottom of the view
   * - :kbd:`e`
     - :kbd:`Shift` + :kbd:`e`
     -
     - Next/previous error
   * - :kbd:`w`
     - :kbd:`Shift` + :kbd:`w`
     -
     - Next/previous warning
   * - :kbd:`n`
     - :kbd:`Shift` + :kbd:`n`
     -
     - Next/previous search hit
   * - :kbd:`>`
     - :kbd:`<`
     -
     - Next/previous search hit (horizontal)
   * - :kbd:`f`
     - :kbd:`Shift` + :kbd:`f`
     -
     - Next/previous file
   * - :kbd:`u`
     - :kbd:`Shift` + :kbd:`u`
     -
     - Next/previous bookmark
   * - :kbd:`o`
     - :kbd:`Shift` + :kbd:`o`
     -
     - Forward/backward through log messages with a matching "opid" field
   * - :kbd:`s`
     - :kbd:`Shift` + :kbd:`s`
     -
     - Next/previous slow down in the log message rate
   * - :kbd:`{`
     - :kbd:`}`
     -
     - Previous/next location in history

Chronological Navigation
------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 5 20

   * - Keypress
     -
     - Command
   * - :kbd:`d`
     - :kbd:`Shift` + :kbd:`d`
     - Forward/backward 24 hours
   * - :kbd:`1` - :kbd:`6`
     - :kbd:`Shift` + :kbd:`1` - :kbd:`6`
     - Next/previous n'th ten minute of the hour
   * - :kbd:`7`
     - :kbd:`8`
     - Previous/next minute
   * - :kbd:`0`
     - :kbd:`Shift` + :kbd:`0`
     - Next/previous day
   * - :kbd:`r`
     - :kbd:`Shift` + :kbd:`r`
     - Forward/backward by the relative time that was last used with the goto command.


.. _hotkeys_bookmarks:

Bookmarks
---------

.. list-table::
   :header-rows: 1
   :widths: 5 20

   * - Keypress
     - Command
   * - :kbd:`m`
     - Mark/unmark the top line
   * - :kbd:`Shift` + :kbd:`m`
     - Mark/unmark the range of lines from the last marked to the top
   * - :kbd:`Shift` + :kbd:`j`
     - Mark/unmark the next line after the previously marked
   * - :kbd:`Shift` + :kbd:`k`
     - Mark/unmark the previous line
   * - :kbd:`c`
     - Copy marked lines to the clipboard
   * - :kbd:`Shift` + :kbd:`c`
     - Clear marked lines

Display
-------

.. list-table::
   :header-rows: 1
   :widths: 5 20

   * - Keypress
     - Command
   * - :kbd:`?`
     - View/leave builtin help
   * - :kbd:`q`
     - Return to the previous view/quit
   * - :kbd:`Shift` + :kbd:`q`
     - Return to the previous view/quit while matching the top times of the two views
   * - :kbd:`a`
     - Restore the view that was previously popped with 'q/Q'
   * - :kbd:`Shift` + :kbd:`a`
     - Restore the view that was previously popped with 'q/Q' and match the top times of the views
   * - :kbd:`Shift` + :kbd:`p`
     - Switch to/from the pretty-printed view of the displayed log or text files
   * - :kbd:`Shift` + :kbd:`t`
     - Display elapsed time between lines
   * - :kbd:`t`
     - Switch to/from the text file view
   * - :kbd:`v`
     - Switch to/from the SQL result view
   * - :kbd:`Shift` + :kbd:`v`
     - Switch to/from the SQL result view and move to the corresponding in the
       log_line column
   * - :kbd:`p`
     - Toggle the display of the log parser results
   * - :kbd:`Tab`
     - In the log/text views, focus on the configuration panel for editing
       filters and examining the list of loaded files.  In the SQL result view,
       cycle through columns to display as bar graphs
   * - :kbd:`Ctrl` + :kbd:`l`
     - Switch to lo-fi mode.  The displayed log lines will be dumped to the
       terminal without any decorations so they can be copied easily.
   * - :kbd:`Ctrl` + :kbd:`w`
     - Toggle word-wrap.
   * - :kbd:`Ctrl` + :kbd:`p`
     - Show/hide the data preview panel that may be opened when entering
       commands or SQL queries.
   * - :kbd:`Ctrl` + :kbd:`f`
     - Toggle the enabled/disabled state of all filters in the current view.
   * - :kbd:`x`
     - Toggle the hiding of log message fields. The hidden fields will be
       replaced with three bullets and highlighted in yellow.
   * - :kbd:`=`
     - Pause/unpause loading of new file data.
     
Histogram
---------

.. list-table::
   :header-rows: 1
   :widths: 5 20
   
   * - Keypress
     - Command   
   * - :kbd:`i`
     - Switch to/from the histogram view
   * - :kbd:`Shift` + :kbd:`i`
     - Switch to/from the histogram view but jump to timestamp of log line at top of screen
   * - :kbd:`z`
     - Zoom in temporially (each line covers smaller breadth of time) when in Histogram view
   * - :kbd:`Shift` + :kbd:`z`
     - Zoom out temporially (each line covers more time) when in Histogram view

Session
-------

.. list-table::
   :header-rows: 1
   :widths: 5 20

   * - Keypress
     - Command
   * - :kbd:`Ctrl` + :kbd:`R`
     - Reset the current :ref:`session<sessions>` state.

Query Prompts
-------------

.. list-table::
   :header-rows: 1
   :widths: 5 20

   * - Keypress
     - Command
   * - :kbd:`/`
     - Search for lines matching a regular expression
   * - :kbd:`;`
     - Open the :ref:`sql-ext` to execute SQL statements/queries
   * - :kbd:`:`
     - Execute an internal command, see :ref:`commands` for more information
   * - :kbd:`\|`
     - Execute an lnav script located in a format directory
   * - :kbd:`Ctrl` + :kbd:`]`
     - Abort the prompt

Customizing
-----------

You can customize the behavior of hotkeys by defining your own keymaps.
Consult the :ref:`Keymaps<keymaps>` configuration section for more information.
