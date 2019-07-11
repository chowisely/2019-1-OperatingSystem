## Linux Kernel Module Rootkit



> ### Background: Operating System Structure
>
> ---
>
> * Operating System Design
>
>   * Goals
>
>     User Goals: convenient to use, easy to learn and use, reliable and safe and fast
>
>     System Goals: easy to design, implement and maintain
>
> 
>
> * Operating System Structure
>
>   A common approach is to partition tasks into small components, or modules, rather than have one monolithic system.
>
>   1. Simple Structure
>
>      ​	In *MS-DOS*, the interface and levels of functionality are not well separated. Thus, application programs are able to access the I/O routines directly but there is a possibility that the system can be damaged to errant programs.
>
>      ​	*Unix* was initially limited by hardware functionality. It consists of kernel and system programs. Everything below the system call interface and above the physical hardware is the kernel. The kernel provides the file system, CPU scheduling, memory management, and other operating system functions to be combined into one level. The monolithic system structure was difficult to implement and maintain. However, it had a distinct performance advantage in terms of little overhead.
>
>      ![image-20190709150859284](/Users/jeonginy/Library/Application Support/typora-user-images/image-20190709150859284.png)
>
>   2. Layered Appraoch
>
>      ​	Operating systems can be broken into pieces so that it retains much greater control over the computer and applications: it gives us more freedom in changing and creating modular operating systems as well as information hiding. The bottom layer is the hardware while the highest is the user interface. A typical *layer* is an implementation of an abstract object made up of data and the operations.
>
>   3. Microkernels
>
>      ​	*Microkernel* removes all nonessential components from the kernel resulting in a smaller kernel. The main function is communication through message passing between a client program and various services that are also running in user space. The advantage is to extend the system easier. All new services are added to user space and do not require modification of the kernel. However, it leads to increased overhead.
>
>   4. Modules
>
>      ​	The best is a *loadable kernel module*. The kernel has a set of core components and links in additional services via modules. What it is different from a layered system is any module can call any other module.
>
>   5. Hybrid Systems
>
>      ​	*Hybrid systems* are combined different structures. Linux and Solaris are monolithic but they are modular so that new functionality can be dynamically added to the kernel.
>
>      



> ### Linux Kernel Module
>
> ---
>
> * LKM
>
>   Linux 운영체제는 커널 모듈의 모음이다. 필요에 따라 해당 모듈이 load되고, 그 기능이 더 이상 필요하지 않다면 unload되기 때문에 메모리를 효율적으로 사용할 수 있다.
>
> 
>
> * Rootkit
>
>   컴퓨터 소프트웨어 중에서 시스템에 악의적으로 접근해서 제어권 혹은 관리자 권한을 획득할 목적으로 설치되는 것. 한 번 설치되면, 침입을 숨길 수도 있다.
>
>   
>
> * LKM + Rootkit
>
>   LKM Rootkit은 Rootkit을 커널 모듈로 만들어 커널 영역에 올리는 것이다. 이 모듈이 커널에 올라가게 되면, 정상적인 system call을 가로채서 공격자가 만든 system call을 실행하도록 한다. 더 자세히 말하자면, 'sys_call_table'에 원래 system call의 주소를 공격자가 만든 system call의 주소로 바꾸어서 해당 routine이 실행된다.
>

  int my_write () {
    ...
   }
  int init_module () {
    orig_sys = sys_call_table[__NR_write];
    sys_call_table[__NR_write] = my_write;
  }
  
>
>   이러한 점에서 LKM이 커널을 수정하는 데 편리하지만 모듈을 load할 때 확장된 권한을 요구함으로써 공격자들에게 남용될 수 있다.
>
>   



> ### Let's Make Our Own LMK rootkit 'Dogdoor'
>
> ---
>
> > * A user level program *bingo.c* used to communicate with dogdoor must satisfy the following requirements:
> >   1. Communicate with via /proc/dogdoor
> >   2. Devise a small protocol for the communication
>
> 
>
> > * *dogdoor.ko* requires the following main functionalities:
> >
> >   1. Log  the names of files that a user has accessed.
> >
> >      The user specifies a user by its username to bingo.
> >
> >      For the given user, dogdoor records the names of files that the user recently opens.
> >
> >      When the user request, bingo retrieves the lists and prints it to the user.
> >
> >   2. Prevent a kill to a specified process.
> >
> >      The user specifies a process ID number to bingo.
> >
> >      Then, dogdoor makes no other process kill the specified process, until the user commands to release this immortality.
> >
> >   3. Hide the dogdoor module from the module list.
> >
> >      Once the user gives a command, dogdoor makes itself disappear the *lsmod* result.
> >
> >      Once the user gives a command again, dogdoor makes itself appear again.
> >
> >   4. Create a text interface /proc/dogdoor.
> >
> >      
