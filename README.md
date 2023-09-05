## Transparent Checkpointing

* **Makefile** is provided with working 'build', 'check', and 'clean' commands
* `ckpt.c` sets `LD_PRELOAD` environment variable before launching user program
* `libckpt.c` registers signal handler for `SIGUSR2`:
  - Signal handler saves context
  - Signal handler reads and saves memory segment headers
* `readckpt.c` correctly reads checkpoint file format and outputs context and memory segment information
* `restart.c`: 
  - Reads checkpoint file
  - Maps memory segments
  - Writes to memory segments
  - Reads context
  - Calls `setcontext`
