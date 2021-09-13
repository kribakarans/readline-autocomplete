# Readline-autocomplete
Auto complete shell command line on TAB key press with GNU readline library.

**Sep 13 2021 10:10**
- Auto complete shell command on TAB key press
- Follow below JSON syntax while adding new command line
- Dependencies: [frozen](https://github.com/cesanta/frozen) json library ```json.c```

**Add new command properties in below format in commands.json**
```
{ command-name : [{
                    "type" : "command-type",
                    "opts" : [ "option1", "option2", "option3" ]
                 }}
```
```
Types: CMD   - Standalone Commands (no options)
       FCTL  - Local File Control commands
       CFCTL - Cloud File Control commands
       UCTL  - User Control commands
       DCTL  - Domain Control commands
```
