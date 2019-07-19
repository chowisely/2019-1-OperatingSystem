## Online Judge Network System(InstaGraP)



> ### Overview
>
> ---
>
> * It is a network system that runs tests on a programming assignment submission to give instant feedback.
>
>   
>
> * You should use the following techniques:
>
>   - Process control
>   - Signal handling
>   - Inter-process communication using pipe
>   - Socket programming
>   - Multithreaded programming
>
>   
>
> * Components
>
>   * *instagrapd* as a master
>   * *worker* as a sandboxed worker
>   * *submitter* as a client
>
>   
>
> * Users
>
>   * Admin
>
>     An administrator launches *worker* process and *instagrapd* process with test cases.
>
>   * Students
>
>     A student runs *submitter* to turn a source code file in *instagrapd* and gets result messages from the server.
>
>     There may be multiple students each of which has a unique student ID and they can make submissions simultaneously.
>
>     
>
> * Workflow
>
>   Step 1. *submitter* sends a request to *instagrapd* for evaluating a C file with student information and a target program source code file.
>
>   Step 2. *instagrapd* delivers the C file with a test input to *worker*. Then, *worker* builds and runs the C file with the given input, and sends back the output. It repeats this step for each test case.
>
>   Step 3. *submitter* asks *instagrapd* whether the result is ready. If it's ready, *instagrapd* sends a feedback back to *submitter*.
>
> 



> ## Submitter
>
> > #### Usage
> >
> > ---
> >
> > **./submitter -n <IP>:<Port> -u <ID> -k <PW> <File>**
> >
> > > <IP> IP address of *instagrapd*
> > >
> > > <Port> port number of *instagrapd*
> > >
> > > <ID> student ID as a 8 digit number
> > >
> > > <PW> password as a 8 digit alphanumeric string
> > >
> > > <File> a target C source code file
>
> > #### Behaviors
> >
> > ---
> >
> > *submitter* connects to *instagrapd* in TCP.
> >
> > At first connection, *submitter* transmits a request for evaluating a target source code file to *instagrapd*.
> >
> > Then, it frequently connects with *instagrapd* to receive the feedback from *instagrapd*. Once received, prints out to the screen.



> ## Instagrapd
>
> > #### Usage
> >
> > ----
> >
> > **./instagrapd -p <Port> -w <IP>:<WPort> <Dir>**
> >
> > > <Port> port for listening of *instagrapd*
> > >
> > > <IP> IP address of *worker*
> > >
> > > <WPort> port of *worker*
> > >
> > > <Dir> a path of testcase directory
>
> > #### Testcase
> >
> > ---
> >
> > A testcase directory always contains 20 files whose names are *1.in*, *1.out*, *2.in*, *2.out*, …, *10.in* and *10.out*.
> >
> > *n.in* is a test input and *n.out* is the expected output.
>
> > #### Behaviors
> >
> > ---
> >
> > *instagrapd* listens at a port to one or multiple *submitter*.
> >
> > It requests *worker* to run the target program with a test input at a time.
> >
> > If a student gives a wrong password(different from the one given at the submission), it rejects a request.
> >
> > Once all testing is done, it answers to *submitter* by giving the number of test cases that the target program passes. Otherwise, sends back the build failure or timeout message.



> ## Worker
>
> > #### Usage
> >
> > ---
> >
> > **./worker -p <Port>**
> >
> > > <Port> port for listening
>
> > #### Behaviors
> >
> > ---
> >
> > *worker* receives a pair of a target C file and a test input file.
> >
> > Then, it builds the C file with the input. If fails, it should send the failure message. Once build succeeds, it runs the target program with the input and sends the output back to *instagrapd*. All the input and the output are from standard streams.
> >
> > If the program execution takes more than 3 seconds, *worker* stops the execution and returns timeout message to *instagrapd*.
> >
> > *worker* should clean up everything related to the target program after each test so that it can work as a sandbox.





