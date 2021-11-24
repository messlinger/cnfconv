# cnfconv
### Canberra CNF to ASCII file conversion

Canberra CNF files store spectral measurement data recorded with the Genie 2000
radiation spectroscopy software in an undocumented binary format. The cnf2txt
converter extracts the spectral data from a cnf file and stores it in a human-readable
text format.

Since the conversion is based on reverse-engeneering of only a small set of sample
files, it may not work correctly for your CNF files. If you come across a file that
does not convert correctly, I would be obliged if you could send me copy.


### Files

* `cnf2txt.exe` Executable binary (Windows)  
* `cnf2txtall.bat` 	Batch file (Windows). Converts all .cnf files in the current directory.  
* `cnf2txtall` 		Batch file (Linux). Converts all .cnf files in the current directory.  
* `cnf2txt.c` 		Source code (system independent).  


### Installation (Windows)

Copy the files `cnf2txt.exe` and `cnf2txtall.bat` to the Windows system path
(typically `C:\WINDOWS`) or to the folder containing the data files.


### Installation (Linux)

    gcc -o cnf2txt.bin cnf2txt.c
    chmod +x cnf2txtall.sh
    sudo cp cnf2txt.bin cnf2txtall.sh /usr/local/bin
    sudo ln -s /usr/local/bin/cnf2txt.bin /usr/local/bin/cnf2txt


### Usage (command prompt):

* Convert a single file:
    `cnf2txt input_file [output_file]`
* Convert all files in the current directory:
`cnf2txtall`


### Usage (Windows explorer):

* Convert a single file: Drag'n'drop the input .cnf file onto `cnf2txt.exe`.
* Convert all files in the current directory: Double click on `cnf2txtall.bat`

**Note**: Make sure your explorer does not hide the file extensions. 

