# proj2-OSWIZARDS
Please view this readme in RAW format! It's prettier!

Group Members: Pietro Candiani, Mattan Peleah, Judas Smith

Make instructions:
    part 1:
        make, then view the .trace files generated
    part 2:
        make, then insmod my_timer.ko
    part 3: 
        make, then insmod barstool.ko, then to initialize or close bar use consumer, to spawn customers use producer
    
None of the rust extra credit was completed. 

Division of Labor:
    Part 1: Pietro Candiani, Mattan Peleah, Judas Smith
    Part 2: Pietro Candiani, Mattan Peleah, Judas Smith
    Part 3.1: Judas Smith
    Part 3.2: Pietro Candiani
    Part 3.3: Mattan Peleah
    Part 3.4: Pietro Candiani, Mattan Peleah


Bugs / Incomplete portions:
    No known bugs.

Special considerations:
    please

Files:
    Part1:
        Makefile    - makes empty and part1 and runs them, outputting to .trace files
        empty.c     - empty file 
        part1.c     - file with 4 syscalls
    Part2:
        Makefile    - makes my_timer
        my_timer.c  - example timer program
    Part3:
        Makefile    - makes barstool, consumer, and producer
        barstool.c  - contains main logic of program
        sys_call.c  - syscalls
        wrappers.h  - provided file
        consumer.c  - provided file
        producer.c  - provided file