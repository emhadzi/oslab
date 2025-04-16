# 3η Εργαστηριακή Άσκηση

Επεκτείνουμε το πρόγραμμα του Eρωτήματος 2 ώστε να δημιουργεί P διεργασίες παιδιά 
οι οποίες αναζητούν παράλληλα τον χαρακτήρα στο αρχείο.
Η γονεϊκή διεργασία συλλέγει και τυπώνει το συνολικό αποτέλεσμα. Επίσης,
όταν το πρόγραμμά δέχεται Control+C από το πληκτρολόγιο αντί να τερματίζει, τυπώνει το συνολικό αριθμό
διεργασιών που αναζητούν το αρχείο.

Το πρόγραμμά μας δέχεται 3 ορίσματα:
- Το αρχείο στο οποίο γίνεται η αναζήτηση
- Τον χαρακτήρα προς αναζήτηση
- Τον αριθμό P των διεργασιών-παιδιών

## Υπολογισμός μεγέθους αρχείου

Πρώτα βρίσκουμε το μέγεθος του αρχείου για να κατανείμουμε κατάλληλα τον φόρτο εργασίας σε κάθε παιδί.
```c
int fdr = open(argv[1], O_RDONLY | O_CREAT, 0666);	
if(fdr < 0){
	printf("Problem opening target file\n");
	return -1;
}	
int sz = lseek(fdr, 0, SEEK_END);
printf("Target file size: %d\n", sz);
close(fdr);	
```
Επίσης κλείνουμε το αρχείο καθώς στην παρούσα υλοποίηση οι διεργασίες δεν μοιράζονται file descriptors,
όπως θα εξηγήσουμε παρακάτω.

## Υπολογισμός φόρτου εργασίας για κάθε παιδί

Υπολογίζουμε τον αριθμό σε bytes τον οποίο πρέπει να διαβάσει κάθε παιδί. Αν ο αριθμός των παιδιών δεν δοθεί ως όρισμα,
χρησιμοποιούμε το 2 ως προεπιλογή.
```c
workers = (argv[3] == NULL ? 2 : atoi(argv[3]));	
int workerSz = sz / workers, rem = sz % workers;
printf("Assigning chunks of up to %d bytes to each worker\n", workerSz);
```

## Εγκατάσταση εξυπηρετητή σήματος SIGINT για τα παιδία

Έπειτα εγκαθιστούμε τον δικό μας άδειο signal handler του σήματος SIGINT για τα παιδιά, καθώς δεν θέλουμε να γίνεται
τίποτα όταν λαμβάνουν το σήμα.
```c
//block SIGINT for all processes
sigset_t block;
sigemptyset(&block);
sigaddset(&block, SIGINT);	
sigprocmask(SIG_BLOCK, &block, NULL);
```

Σημείωση:  Κάθε διεργασία που δημιουργείται με `fork`, αφού έχει εγκατασταθεί ο εξυπηρετητής, θα κληρονομήσει επίσης τον ίδιο εξυπηρετητή.
Οι εξυπηρετητές μεταβαίνουν στην προεπιλεγμένη συμπεριφορά τους μόνο αν μετά το fork κάνουμε execv, ή τους αλλάξουμε χειροκίνητα. 
Επίσης στη γενική περίπτωση αν στείλουμε σήμα SIGINT πριν την εγκατάσταση του εξυπηρετητή το πρόγραμμα θα εκτελέσει την προεπιλεγμένη λειτουργία, δηλαδή θα τερματίσει.

## Δημιουργία παιδιών και σωληνώσεων επικοινωνίας

```c
pid_t pid[workers];

int pipefd[workers][2];

int ind = 0, p = 1;
for(; ind < workers && p > 0; ind++){
	if(pipe(pipefd[ind]) == -1){
		printf("Error creating pipe for %d worker\n");
		p = -1;
	}	
	else{
		fcntl(pipefd[ind][0], F_SETFL, O_NONBLOCK);		
		p = fork();
		if(p > 0)
			pid[ind] = p;
	}	
}
if(p < 0){
	//only parent process will land here if "ind" child is not created successfully
	//children [0, ind-2] should terminate
	printf("Error creating worker %d, terminating the rest\n", ind);
	for(int i = 0; i < ind-1; i++)
		kill(pid[i], SIGTERM);
	return -1;
}

```

Αφού έχουμε εγκαταστήσει τον εξυπηρετητή προσπαθούμε να δημιουργήσουμε P παιδία και P pipes με τα οποία θα μπορούμε να λαμβάνουμε αποτελέσματα από αυτά.
Αν κάτι πάει λάθος, η μεταβλητή p θα λάβει αρνητική τιμή, το πρόγραμμα θα ενημερώσει τον χρήστη για το σφάλμα και θα τερματίσει όλες τις διεργασίες που έχουν ήδη δημιουργηθεί.

Με την κλήση συστήματος pipe δημιουργούμε έναν buffer, τον οποίο διαχειρίζεται το λειτουργικό, ο οποίος επιτρέπει εγγραφή πληροφοριών στο ένα άκρο του (από μια διεργασία) και ανάγνωση της πληροφορίας από το άλλο άκρο του (από μια άλλη διεργασία)
Ο descriptor `pipefd[ind][0]` αντιστοιχεί στο άκρο ανάγνωσης, τον οποίο χρησιμοποιεί ο γονέας για τον έλεγχο των αποτελεσμάτων των παιδιών και ο 
descriptor `pipefd[ind][1]` αντιστοιχεί στο άκρο εγγραφής, τον οποίο χρησιμοποιούν τα παιδία για την εκτύπωση των αποτελεσμάτων.

Εφόσον δεν γνωρίζουμε πότε κάθε παιδί θα εκτυπώσει το αποτέλεσμα του στο pipe, θα θέλαμε ο γονέας να μην περιμένει (block) όταν το pipe είναι άδειο μέχρι να σταλθεί κάποια πληροφορία, ώστε να μπορεί να συνεχίσει με άλλες εργασίες.
Για αυτόν τον λόγο χρησιμοποιούμε την κλήση συστήματος `fcntl(pipefd[ind][0], F_SETFL, O_NONBLOCK);` η οποία δεν "μπλοκάρει" το άκρο ανάγνωσης αν το pipe είναι άδειο.

Τέλος, με την κλήση συστήματος `fork()` δημιουργούμε αλυσιδωτά τα παιδιά (το ένα παιδί δημιουργεί το άλλο) και αποθηκεύουμε το process id τους στον πίνακα `pid`.

## Βρισκόμαστε σε κάποιο από τα παιδιά (p==0)
```c
else if(p == 0){
		int i = ind-1;
		//only child/worker will land here
		printf("Worker %d: Starting\n", ind);
		//close ununsed (by child) reading end of pipe
		close(pipefd[i][0]);	
		//block SIGINT
		//sigprocmask(SIG_BLOCK, &block, NULL);
	
		//create new file descriptor
		fdr = open(argv[1], O_RDONLY, 0666);	
		if(fdr < 0){
			printf("Worker %d could not open target file\n", ind);
			return -1;
		}	

		//deterimine starting and ending positions (inclusive)
		int startPos, endPos;
		if(i < rem){
			startPos = i * (workerSz+1);
			endPos = startPos + workerSz;
		}
		else{
			startPos = rem + i * workerSz;
			endPos = startPos + workerSz - 1;
		}
		int ans = 0;
		char buff[BATCH_SIZE];

		//set fd offset to startPos
		lseek(fdr, startPos, SEEK_SET);

		for(int curPos = startPos; curPos <= endPos;){	
			int readSz = min(BATCH_SIZE, endPos - curPos + 1);	
			read(fdr, buff, readSz);
			usleep(SLEEP_DURATION);
			curPos += readSz;			

			for(int i = 0; i < readSz; i++)
				ans += buff[i] == argv[2][0]; 
		} 		
		close(fdr);

		//write result to pipe
		write(pipefd[i][1], &ans, sizeof(int));
		close(pipefd[i][1]);
		printf("Worker %d: Done\n", ind);		

		return 0;
	}
```

Για καλή πρακτική κλείνουμε το άκρο ανάγνωσης που χρησιμοποιεί μόνο ο γονέας. Έπειτα ανοίγουμε έναν καινούργιο file descriptor για το αρχείο μας, βρίσκουμε το κομματί που πρέπει να διαβάσουμε και μετακινούμε τον δείκτη ανάγνωσης σε εκείνο το σημείο με την εντολή `lseek`.
Έπειτα διαβάζουμε το κομμάτι του αρχείου που μας αντιστοιχεί σε κομμάτια των `BATCH_SIZE`, μετράμε τις εμφανίσεις του ζητούμενου χαρακτήρα, εκτυπώνουμε το αποτέλεσμα στο άκρο εγγραφής του pipe και τερματίζουμε. Ας σημειωθεί πως εφόσον η δημιουργία των παιδιών γίνεται αλυσιδωτά, η μεταβλητή ind ανανεώνεται σωστά, οι αναφορές στα pipes είναι και αυτές σωστές.

Χρήση διαφορετικoύ file descriptor σε κάθε παιδί έναντι διαμοιρασμού κοινού file descriptor:
Θα μπορούσαμε να χρησιμοποιήσουμε έναν κοινό fdr για όλα τα παιδιά αν χρησιμοποιούσαμε μόνο την εντολή `read` καθώς το λειτουργικό 
εξασφαλίζει την ατομική λειτουργία της. Όμως με αυτήν την υλοποίηση δεν θα είχαμε έλεγχο στο ποιο κομμάτι του αρχείου θα διάβαζε κάθε παιδί, καθώς δεν θα μπορούσαμε
να χρησιμοποιήσουμε την εντολή `lseek`, η οποία δεν εκτελείται ατομικά σε συνδυασμό με την `read`. Για αυτόν τον λόγο επιλέξαμε κάθε παιδί να έχει τον δικό του fdr.

## Βρισκόμαστε στον γονέα (p == κάποιο pid)
```c
else{
		//only parent process will land here if all children are created successfully
		active = workers;
		printf("Successfully created %d workers\n", workers);
		//wait for all children to finish and report result
		//if one child fails remember it, after all finish, report it
		int tot = 0;
		bool ok = 1;	
	
		struct sigaction slog;
		slog.sa_handler = handler;
		slog.sa_flags = 0;
		sigemptyset(&slog.sa_mask);
		sigaction(SIGINT, &slog, NULL);		
	
		while(active > 0){
			for(int i = 0; i < workers; i++){
				if(done[i])
					continue;		
				int wstatus;
				sigprocmask(SIG_BLOCK, &block, NULL);
				pid_t pdone = waitpid(pid[i], &wstatus, WNOHANG);
				sigprocmask(SIG_UNBLOCK, &block, NULL);	
				
				if(pdone == pid[i]){
					int res;
					int sz = read(pipefd[i][0], &res, sizeof(int));
					if(sz > 0){
						close(pipefd[i][0]); 	
						printf("Worker (%d): Successfully accounted\n", i+1);
						tot += res;
					}
					else{
						printf("Worker (%d): Failed\n", i+1);
						ok = 0;
					}
					done[i] = 1;
					active--;
				} else if (pdone == -1) {
						printf("Worker (%d): Failed\n", i+1);
						ok = 0;
					}
				}
			}
		}

		if(!ok) printf("Cannot provide final result because some worker failed\n");
		else printf("Final result: %d\n", tot);
	}
```

Το πρώτο πράγμα που κάνουμε στον γονέα είναι να εγκαταστήσουμε τον εξυπηρετητή που εκτυπώνει τον αριθμό των ενεργών παιδιών.
Έπειτα, διατρέχουμε όλα τα παιδιά και περιμένουμε (non blocking) μέχρι κάποιο να τερματίσει με την κλήση `waitpid`. Αν τερμάτισε, δηλαδή `pdone==pid`, καταγράφουμε το αποτέλεσμα και μαρκάρουμε το παιδί για να μην χρειαστεί να το ξαναελέγξουμε `done[i]==1`, επίσης μειώνουμε τα ενεργά παιδιά κατά ένα `active--;`.
Αν υπάρχει κάποιο σφάλμα, δηλαδή `pdone == -1` ή `sz==0`, ενημερώνουμε τον χρήστη. Αν όλα πάνε καλά, και τερματίσει το τελευταίο ενεργό παιδί, εκτυπώνουμε το συνολικό αποτέλεσμα.

Ας σημειωθεί πως όταν περιμένουμε για κάποιο παιδί να τερματίσει με την κλήση `waitpid`, απενεργοποιούμε τον signal handler του SIGINT, 
καθώς αν συνέπεφτε το SIGINT την ώρα που εκτελούσαμε την κλήση waitpid, αυτή θα επέστρεφε -1, το οποίο θα μπορούσαμε να διαχειριστούμε κοιτώντας αν `errno == EINTR`. Για ευκολία όμως επιλέξαμε να απενεργοποιήσουμε προσωρινά τον signal handler.

 

