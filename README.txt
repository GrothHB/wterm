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
	KEYBINDINGS:
		The following control characters are natively handled:
		Ctrl+L      Clears the terminal screen (Form Feed).
		Ctrl+R      Toggles the Reverse Search mode.
		Tab         Invokes the completion mechanism.

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

TECHNICAL NOTES
        Terminal Window Scaling:
                The program dynamically queries terminal dimensions via the
                TIOCGWINSZ ioctl(2). The OPENBSD_MAX macro enforces a strict upper
                bound of 80 columns. If the physical terminal is narrower, the
                internal screen limit scales down automatically, ensuring safe
                line wrapping without overflowing the rendering buffer.
		NOTE: the renedring text not always works. If it fails use
		Ctrl+L.

        Input Dispatching:
                Input characters are handled by an array of function pointers mapped
		directly to the 128 ASCII characters, avoiding iterative checks during
		raw input processing. It doesn't handle all UTF-8 encoding (e.g. wchar_t)
		Asynchronous sequence decoding is managed via poll(2) with custom
		timeouts to safely process ANSI escape codes without thread blocking.

        Execution and IPC:
                wterm dispatches completed command buffers to a child shell
                process via fork(2) and execve(2). To maintain environmental
                synchronization (e.g., preserving `cd` state), it uses a
                side-channel IPC mechanism:
                A unidirectional pipe is bound to file descriptor 3 of the child.
                The child shell communicates state changes back to the parent by
                appending `;set >&3` to the executed command. The parent parses 
                these key-value pairs in chunks and stores them in an internal hash 
                table (1024 buckets) and refreshes them or adds new ones via setenv(3).
		URGENT NOTE:
			wterm is designed mostly to be compatible with small POSIX
			shells (e.g. dash). It always parses shell variables that are
			in the form of `key='value'` or `key=value`, but it doesn't
			parse other types of variables like functions, which may lead
			to variable corruption.
                
        Argument Limit Enforcement:
                Before calling execve(2), the wrapper dynamically queries the
                operating system's maximum argument length using sysconf(_SC_ARG_MAX)
                to ensure safe execution and prevent buffer overflows. The limit it checks
		is 1/4 of the current ARG_MAX to leave place for **envp.
