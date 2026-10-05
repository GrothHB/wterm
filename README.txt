NAME:
        wterm - raw-mode terminal wrapper and command-line editor.

DESCRIPTION:
        This is a simple terminal wrapper, born from using dash.
        It is an educational project for better understanding of general 
        knowledge of C and the UNIX environment. It takes control of the 
        terminal in raw mode.
        Program execution is handled by the shell pointed to by the global
        variable "$SHELL". If the variable is not set, the default is "/bin/sh".
        The program possesses a simple GUI-like format, in the understanding 
        that it will pad a line break limit (by default 80 characters, 
        including PS1).
        The command limit is set by the macro `MAX_CANON' from <limits.h>.
        For a more technical description, see "TECHNICAL NOTES" below.

MOVEMENT:
        The program handles arrow movement, allowing character insertion inside 
        strings. Up and down arrows are reserved for going through the history 
        of commands saved by the program.

PROMPT OPTIONS:
        The prompt is based on the variable "$PS1". If it is NULL, it is set to `$'.
        The length limit of "$PS1" is 21 characters. If it exceeds this, the 
        program won't truncate it, but will set the prompt to `$' instead.

UTILITIES:

        HISTORY SEARCH:
                History search is controlled by the up and down arrows. It displays
                history starting from the last executed command. It only acts 
                when the cursor is on the last character of the current command. 
                Until the user presses enter, the position of the last retrieved 
                command is remembered, so after manipulation of the current 
                command, it can be searched from the same point that was last used.
                Example:
                Let `[]' mean the cursor.
                $ ls -1F[] will work, but $ ls -[1]F won't.

        TAB COMPLETION:
                Tab completion searches for matching strings based on cursor location.
                If the cursor is located on the first string in the CLI, it searches 
                every path in the "$PATH" variable until it finds one or more 
                matching programs. If the number of matching programs exceeds 60, 
                it will ask for extra confirmation to print them all. It also 
                searches these paths if the cursor is located right after `|', 
                `;', `<', `&', or `('.
                In case the cursor is located after the first string or `>', it 
                searches for files in the current directory.
                It understands prefixes like `~/' or `./' as home directory and 
                current directory.
                It will not attempt to search for completion if the cursor is 
                located inside quotes (`\'' or `\"').
                When completing a string, if it is the last word and not a directory, 
                or if it is inside a command, it adds a single space and puts the 
                cursor right after it. In other cases, the cursor is located right 
                after the completed word.
                If there is more than one match, it completes the string up to the 
                common prefix of the found strings.
	REVERSE SEARCH:
		When reverse search mode is enabled, it changes the `$' sign to `?' in
		the prompt. If the command is empty and enter is pressed, it prints
		the numbered history from oldest to newest command.
		Commands for this mode are:
		
		s pattern [pattern [pattern [...]]]
			It searches through the history for a command that matches all
			patterns. The order of the patterns is irrelevant.
			
		ss pattern
			It treats the pattern as a whole string. It includes everything
			that is located inside the pattern, spaces, quotes, and other
			special characters included.
			
		t n
			It takes the command which corresponds to the number n from the 
			history, puts it as the current command, and turns off reverse 
			search mode.
