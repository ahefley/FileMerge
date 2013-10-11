// FileMerge.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <ctime>
#include <string>
#include <iostream>
#include <time.h>
#include <fstream>
#include <windows.h> // for SYSTEMTIME
#include <sstream>
using namespace std;

#define MAX_LINE_LENGTH 1024
#define MAX_FIELD_NUM 60
#define LOG_FILE_NAME "FileMerge-log.txt"
char SourcePath[256];                 // The path to the source files specified in the config file
char TargetPath[256];                 // The path to the target files specified in the config file
char ArchivePath[256];                // The path to the archived files (inside the Target path)
char MasterFileName[256];             // The name of the master file specified in the config file
char MasterFileFullName[256];         // The full path + name for the master file
char ChangeFileName[256];             // The name of the change file specified in the config file
char ChangeFileSourceFullName[256];   // The full path + name for the change file in the source directory
char ChangeFileTargetFullName[256];   // The full path + name for the change file in the target directory
char TempFileFullName[256];           // The full path + name for the temporary file used during processing
int MasterFileUniqueIDField;
int ChangeFileUniqueIDField;
FILE* log_file;
string FieldColumns[MAX_FIELD_NUM];
string FolderDate;
string ChangeFileArchive;



int write_to_log(string line)
{
	std::stringstream sstr;

	time_t t = time(0);   // get time now     
	struct tm * now = localtime( & t );     
	sstr << (now->tm_year + 1900) << '-' << (now->tm_mon + 1) << '-' <<  now->tm_mday << '-'<< now->tm_hour << ':' << now->tm_min << ':' << now->tm_sec;

	fputs(sstr.str().c_str(), log_file);

	fputs("\t", log_file); 
	fputs(line.c_str(), log_file);
	return fputs("\n", log_file);

	return 0;	
}

int exit_program()
{
	write_to_log("Aborting File Merge");
	write_to_log("------------------------------------------------");
	exit(-1);

}



// get_change_type() - For a given line of characters taken from an input file, 
// this function determines the type of change being made.  The result is placed
// in the 'type' variable and is one of three strings:
//   "CHG"
//   "ADD"
//   "TRM"
// This routine assumes the first column of the data (everything before the first
// comma in the CSV line) is the change type.
// The return value indicates whether the change type was correctly parsed (0) or 
// a problem was found in the input line (-1).
//
int get_change_type(char* line, char* type)
{
	int result = 0;
	int total_chars = 0;

	// Copy first characters in line into 'type' array until we hit the comma
	while ((*line != ',') && (total_chars++ < MAX_LINE_LENGTH))
		*type++ = *line++;
	if (total_chars >= MAX_LINE_LENGTH)
		result = -1;
	else
	{
		*type = '\0';
		result = 0;
	}
	return result;
}


// get_unique_id() - For a given line of characters taken from an input file, 
// this function determines the unique ID for the record.  The result is placed in 
// the 'id' variable as a string.
// The field_num parameter specifies which field in the line contains the unique ID.
// This leaves it up to the caller to specify the "format" of the input line.
// It is assumed that the fields are in full quotes between the comma delimiters.  This
// allows us to support commas inside fields (e.g. "16 Montana Street, Suite 49").
// The return value indicates whether the ID was correctly parsed (0) or a problem
// was found in the input line (-1).
//
int get_unique_id(char* line, char* id, int field_num)
{
	int result = 0;
	int commas_found = 0;
	int total_chars = 0;
	bool inside_quotes = false;
	char* idptr = id;
	*idptr = 0;

	// Skip fieldnum-1 fields, watching to see that we don't run past the buffer end
	while ((commas_found < (field_num - 1)) && (total_chars < MAX_LINE_LENGTH))
	{
		if (*line == ',')
			if (!inside_quotes)
				commas_found++;
		if (*line == '"')
			inside_quotes = !inside_quotes;
		line++;
		total_chars++;
	}

	// Copy the next (fieldnumth) field into the id
	while ((*line != ',') && (total_chars < MAX_LINE_LENGTH))
	{
		*idptr++ = *line++;
		total_chars++;
	}

	// Check for errors & empty strings and report the result
	if (id[0] == 0 || (id[0] == '"' && id[1] == '"'))
		result = -1;
	else
	{
		*idptr = '\0';
		result = 0;
	}
	return result;
}


// get_unique_id_in_master_file() - For a given line of characters taken from
// a master input file, this function determines the unique ID for the record.
// The result is placed in the 'id' variable as a string.
// This routine uses the EmployeeNbr field as the unique ID, and it assumes the 10th 
// column of data (i.e. 10th field in the CSV line) contains the ID.
// The return value indicates whether the ID was correctly parsed (0) or a problem
// was found in the input line (-1).
//
int get_unique_id_in_master_file (char* line, char* id)
{
	int result = 0;

	result = get_unique_id(line, id, MasterFileUniqueIDField);

	return result;
}


// get_unique_id_in_change_file() - For a given line of characters taken from
// a change input file, this function determines the unique ID for the record.
// The result is placed in the 'id' variable as a string.
// This routine uses the EmployeeNbr field as the unique ID, and it assumes the 16th 
// column of data (i.e. 16th field in the CSV line) contains the ID.
// The return value indicates whether the ID was correctly parsed (0) or a problem
// was found in the input line (-1).
//
int get_unique_id_in_change_file (char* line, char* id)
{
	int result = 0;

	result = get_unique_id(line, id, ChangeFileUniqueIDField);

	return result;
}


// scrub_filename() - Fixes a filename by turning all instances of '\' into '\\'.
//
void scrub_filename(char* name)
{
	char buf[256];

	strcpy(buf, name);
	char* bufptr = buf;
	char* nameptr = name;
	while (*bufptr != '\0')
	{
		if (*bufptr == '\\')
			*nameptr++ = '\\';  // Duplicate the slash
		*nameptr++ = *bufptr++;
	}
	*nameptr = '\0';
}


// output_master_line() - Writes a master line to the given file.
// This routine sends the entire line (all fields) to the output file,
// because the input line is already in the proper format for a Master
// file.
// This routine returns 0 if there are no errors or -1 if any issues are 
// encountered.
//
int output_master_line(char* line, FILE* file)
{
	int result = 0;
	result = fputs(line, file);

	return result;
}


// output_change_line() - Writes a change line to the given file.
// This routine sends only certain fields in the line to the output file,
// as non-essential fields in the change file are stripped out.
// The fields kept (and their zero-based indices) are:
//   UserId(1), CompanyCode(2), CompanyDescr(3), Plant(4), BusinessUnit(5),
//   LocationCity(10), LocationState(11), LocationZipCode(12), LocationCountry(13),
//   EmployeeNbr(15), DateOfBirth(23), PayType(28), JobCode(30), EmployeePlant(32),
//   Manager(35), Executive(36), EmploymentType(43), LengthOfService(44),
//   DivisionDescription(45), JobFamily(46), JobFunction(47), PlantDescription(48),
//   CostCenter(49), DepartmentDesc(50)
// This routine returns 0 if there are no errors or -1 if any issues are 
// encountered.
//
int output_change_line(char* line, FILE* file)
{
	int result = 0;

	//start counting at 0 since want to ignore the first column (which is CHG, ADD, etc)
	int field_num = 0;
	bool inside_quotes = false;
	string output_lineStr[MAX_FIELD_NUM];
	string lineStr = line;
	string aField = "";

	for (int i=0; i<(int)lineStr.length(); i++) {

		//s << "Field #" << field_num << " - " << aField

		//end of a field
		if (lineStr[i]==',' && !inside_quotes) {

			for (int p=0; p<MAX_FIELD_NUM; p++) {

				if (FieldColumns[p]!="" && FieldColumns[p].find("\"",0) == string::npos && atoi(FieldColumns[p].c_str())==field_num) {
					//cout << FieldColumns[p] << endl;
					output_lineStr[p]=aField;
					//cout << "Found field " << field_num << " with value " << aField << endl;
					break;
				}
			}
			aField = "";
			field_num++;
		}

		//if we see a quote
		else if (lineStr[i]=='"') {
			inside_quotes = !inside_quotes;
		}

		//otherwise, add character to the field
		else {
			aField += lineStr[i];
		}

	}

	//create the output line
	string outputStr = "";
	for (int i=0; i<MAX_FIELD_NUM; i++) {
		if (FieldColumns[i]!="") {

			//deal with any complex lines
			if (FieldColumns[i].find("\"",0) != string::npos) {
				//remove quotes
				string desiredText = FieldColumns[i].substr(1,FieldColumns[i].length()-2);
				//cout << desiredText << endl;

				//replace the digits with the appropriate text
				for (int p=0; p<MAX_FIELD_NUM; p++) {
					int digits = desiredText.find(FieldColumns[p]);

					//get next char after match & make sure it's not a digit too
					//prevents '1' from matching with '18'
					char next_char = desiredText[digits+FieldColumns[p].length()];
					//cout << desiredText << "  " << digits << "   " << next_char << "\n";
					if (next_char == '0' || next_char == '1' || next_char == '2' || next_char == '3' || next_char == '4' || next_char == '5' || next_char == '6' || next_char == '7' || next_char == '8' || next_char == '9')
						continue;

					//if the complex line has the # of a desired field, replace it with the desired text
					if (p != i && digits >= 0) {
						//cout << "Replacing " << FieldColumns[p] << " with " << output_lineStr[p] << endl;
						desiredText.replace(digits,FieldColumns[p].length(),output_lineStr[p]);
					}
				}

				output_lineStr[i] = desiredText;
			}

			outputStr += '"';
			outputStr += output_lineStr[i];
			outputStr += '"';
			outputStr += ',';
		}
	}
	//remove trailing comma
	if (outputStr.length() > 0) {
		outputStr = outputStr.substr(0,outputStr.length()-1);
	}
	//cout << outputStr << endl;
	outputStr+="\n";
	// Output the line we've built
	result = fputs(outputStr.c_str(), file);

	return result;
}


// load_config() - Loads the configuration file values into the global state.
// The file must be names FileMerge.cfg and be stored in the same directory as
// the executable.
// At the end of this routine, all global variables for file names and paths
// should be set.
// This routine returns 0 if there are no errors (all necessary values are present
// in the config file) or -1 if any issues are encountered.
//
int load_config()
{
	write_to_log("Loading config file");

	FILE* config_file;
	config_file = fopen(".\\FileMerge.cfg", "r");
	if (config_file == NULL)
	{
		printf("ERROR: Cannot open Configuration File (FileMerge.cfg) for reading.  Exiting.\n");
		write_to_log("ERROR: Cannot open Configuration File (FileMerge.cfg) for reading.  Exiting.");
		exit_program();
	}

	// Initialize result values
	SourcePath[0] = 0;
	TargetPath[0] = 0;
	MasterFileName[0] = 0;
	ChangeFileName[0] = 0;
	MasterFileUniqueIDField = 0;
	ChangeFileUniqueIDField = 0;
	int FieldColumnsCtr = 0;
	FolderDate = "";
	ChangeFileArchive = "";


	// Read in file one line at a time, filling in result values
	char line[MAX_LINE_LENGTH];
	while (fgets(line, MAX_LINE_LENGTH, config_file))
	{
		string lineStr = line;
		string beforeptr;
		beforeptr = lineStr.substr(0,lineStr.find("=",0));
		if (beforeptr != "")
		{
			string afterptr = lineStr.substr(lineStr.find("=",0)+1);
			//remove line break
			afterptr[afterptr.length()-1]=NULL;

			if (beforeptr=="SourcePath")  {
				strcpy(SourcePath,afterptr.c_str());
			}
			else if (beforeptr=="TargetPath")
				strcpy(TargetPath,afterptr.c_str());
			else if (beforeptr=="MasterFileName")
				strcpy(MasterFileName, afterptr.c_str());
			else if (beforeptr=="ChangeFileName")
				strcpy(ChangeFileName, afterptr.c_str());
			else if (beforeptr=="FolderDate") {
				FolderDate = afterptr;
				//cout << "FolderDate=" << FolderDate << "*" << endl;
			}
			else if (beforeptr=="ChangeFileArchive")
				ChangeFileArchive = afterptr;

			else if (beforeptr.find("Field")==0) {
				int startNum = afterptr.find("(")+1;
				int lengthNum = afterptr.find(")")-startNum;
				FieldColumns[FieldColumnsCtr] = afterptr.substr(startNum, lengthNum);
				//cout << FieldColumns[FieldColumnsCtr] << endl;
				FieldColumnsCtr++;
			}
			else if (beforeptr.find("MasterFileUniqueIDField")==0 ) {
				MasterFileUniqueIDField = atoi(afterptr.c_str());
			}
			else if (beforeptr.find("ChangeFileUniqueIDField")==0 ) {
				ChangeFileUniqueIDField = atoi(afterptr.c_str());
			}
		}
	}

	// Check for missing configuration information
	if (SourcePath[0] == 0)
	{
		printf("ERROR: Missing SourcePath in configuration file.  Exiting.\n");
		write_to_log("ERROR: Missing SourcePath in configuration file.  Exiting.");
		exit_program();
	}
	if (TargetPath[0] == 0)
	{
		printf("ERROR: Missing TargetPath in configuration file.  Exiting.\n");
		write_to_log("ERROR: Missing TargetPath in configuration file.  Exiting.");
		exit_program();
	}
	if (MasterFileName[0] == 0)
	{
		printf("ERROR: Missing MasterFileName in configuration file.  Exiting.\n");
		write_to_log("ERROR: Missing MasterFileName in configuration file.  Exiting.");
		exit_program();
	}
	if (ChangeFileName[0] == 0)
	{
		printf("ERROR: Missing ChangeFileName in configuration file.  Exiting.\n");
		write_to_log("ERROR: Missing ChangeFileName in configuration file.  Exiting.");
		exit_program();
	}
	if (MasterFileUniqueIDField ==0)
	{
		write_to_log("ERROR: Missing MasterFileUniqueIDField in configuration file.  Should have the column number in which the Unique ID can be found in the Master file.  Exiting.");
		exit_program();
	}
	if (ChangeFileUniqueIDField ==0)
	{
		write_to_log("ERROR: Missing ChangeFileUniqueIDField in configuration file.  Should have the column number in which the Unique ID can be found in the Change file.  Exiting.");
		exit_program();
	}
	if (FolderDate.length() <= 0)
	{
		write_to_log("ERROR: Missing FolderDate in configuration file.  Should have the value 'today' if you want to process the change file for today.  Otherwise it should have a date in mmddyyyy format - in other words, the value of the folder in IDMPS.  Exiting.");
		exit_program();
	}
	if (ChangeFileArchive.length() <= 0)
	{
		write_to_log("ERROR: Missing ChangeFileArchive in configuration file.  Should have the value 'copy' if you want to archive the change file by copying it, or the value 'move' if you want to archive the change file by moving it from its current location.  Exiting.");
		exit_program();
	}

	// Add final '\' to path names if necessary
	char* pathptr = SourcePath;
	while (*(pathptr+1) != '\0')
		pathptr++;
	if (*pathptr != '\\')
	{
		*(++pathptr) = '\\';
		*(++pathptr) = '\0';
	}
	pathptr = TargetPath;
	while (*(pathptr+1) != '\0')
		pathptr++;
	if (*pathptr != '\\')
	{
		*(++pathptr) = '\\';
		*(++pathptr) = '\0';
	}

	// Create full names
	strcpy(MasterFileFullName, TargetPath);
	strcat(MasterFileFullName, MasterFileName);

	strcpy(ChangeFileTargetFullName, TargetPath);
	strcat(ChangeFileTargetFullName, ChangeFileName);
	strcpy(TempFileFullName, TargetPath);
	strcat(TempFileFullName, "FileMergeTemp.txt");
	strcpy(ArchivePath, TargetPath);
	strcat(ArchivePath, "archive\\");

	fclose(config_file);

	return 0;
}


// process_changes_and_deletes() - This function applies all changes and deletes listed 
// in the change file to the master file.  It does not apply the additions (this is done
// in a separate routine).
// The algorithm is simple:
//   for each line in Master File
//     look for corresponding unique ID in change file
//     if a match is found
//       if it's a CHG, inject the new (change) line into the temporary output file
//       if it's a TRM, do nothing (do not output to the temporary file)
//       if it's an ADD, report an error (should never happen)
//     else if no match is found
//       output the master line unchanged into the temporary output file
// If any issues are found, we exit the program with an error message.  Otherwise, 0 is returned.
//
int process_changes_and_deletes()
{
	int total_changes=0;
	int total_copies=0;
	int total_deletes=0;

	// Open Master, Change, and Temp files
	FILE* master_file;
	master_file = fopen(MasterFileFullName, "r");
	FILE* change_file;
	change_file = fopen(ChangeFileSourceFullName, "r");
	FILE* temp_file;
	temp_file = fopen(TempFileFullName, "w");
	if (master_file == NULL)
	{
		printf("ERROR: Cannot open Master File (%s) for reading.  Exiting.\n", MasterFileFullName);
		write_to_log("ERROR: Cannot open Master File for reading.  Exiting.");
		exit_program();
	}
	if (change_file == NULL)
	{
		printf("ERROR: Cannot open Change File (%s) for reading.  Exiting.\n", ChangeFileSourceFullName);
		write_to_log("ERROR: Cannot open Change File for reading.  Exiting.");
		exit_program();
	}
	if (temp_file == NULL)
	{
		printf("ERROR: Cannot open Temporary file (%s) for reading.  Exiting.\n", TempFileFullName);
		write_to_log("ERROR: Cannot open Temporary file for reading.  Exiting.");
		exit_program();
	}

	// Read in Master file one line at a time and look for matching lines in Change file
	char master_line[MAX_LINE_LENGTH];
	char master_unique_id[256];
	char change_line[MAX_LINE_LENGTH];
	char change_unique_id[256];
	char change_type[256];
	bool match_found;

	fgets(master_line, MAX_LINE_LENGTH, master_file);  // Get first line (header) and copy it to output
	output_master_line(master_line, temp_file);
	int master_line_num = 1;
	while (fgets(master_line, MAX_LINE_LENGTH, master_file))
	{
		master_line_num++;
		if (get_unique_id_in_master_file(master_line, master_unique_id) != 0)
		{
			printf("ERROR: Could not find unique ID for line in Master File.  Skipping.\n");
			std::stringstream sstr;
			sstr << "ERROR: Could not find unique ID for line # " << master_line_num << " in Master file. Record will be removed from Master file.";
			write_to_log(sstr.str());
			//skip the rest of the while loop and get the next line in the master file.
			continue;
			//exit_program();
		}
		// Look for matching line in Change file
		match_found = false;
		fseek(change_file, 0, SEEK_SET);
		fgets(change_line, MAX_LINE_LENGTH, change_file);  // Get first line (header) and just throw it away
		int change_line_num = 1;
		while (fgets(change_line, MAX_LINE_LENGTH, change_file))
		{
			change_line_num++;

			

			if (get_unique_id_in_change_file(change_line, change_unique_id) != 0)
			{
				//DON'T WRITE TO LOG BECAUSE IT WILL WRITE ONCE FOR EACH LINE IN THE MASTER FILE - i.e. it duplicates
				/*printf("ERROR: Could not find unique ID for line in Change File.  Skipping.\n");
				std::stringstream sstr;
				sstr << "ERROR: Could not find unique ID for line # " << change_line_num << " in Change File. Skipping this line.";
				write_to_log(sstr.str());*/
				//skip the rest of the while loop and get the next line in the change file.
				continue;
				//exit_program();
			}
			
			// See if this line in the Change file has a matching ID to the line in the Master file
			else if (strcmp(change_unique_id, master_unique_id) == 0)
			{
				match_found = true;
				if (get_change_type(change_line, change_type) != 0)
				{
					printf("ERROR: Could not find change type for line in Change File.  Skipping.\n");
					std::stringstream sstr;
					sstr << "ERROR: Could not find change type for line # " << change_line_num << " in Change File. Skipping this line.";
					write_to_log(sstr.str());
					//skip the rest of the while loop and get the next line in the change file.
					continue;					
					//exit_program();
				}
				// If it's a CHG, output the line from the change file instead of the master file line
				if (strcmp(change_type, "\"CHG\"") == 0 || strcmp(change_type, "\"C2E\"") == 0) {
					output_change_line(change_line, temp_file);
					total_changes++;
				}
				// If it's a TRM, don't output any line
				else if (strcmp(change_type, "\"TRM\"") == 0) {
					total_deletes++;
				}
				// If it's an ADD, we have a problem (should never ADD a record already in the file)
				// Will just treat the ADD as a change.
				else if (strcmp(change_type, "\"ADD\"") == 0)
				{
					printf("ERROR: Change file has an ADD operation for a record in the Master file.  Will replace the record in the Master file with the record in the Change file.\n");
					std::stringstream sstr;
					sstr << "ERROR: Change file has an ADD operation on line # " << change_line_num << " for a record that already exists in the Master file.  Will replace the record in the Master file with the record in the Change file.";
					write_to_log(sstr.str());
					output_change_line(change_line, temp_file);
					total_changes++;
					match_found = true;
					//exit_program();
				}
				// Otherwise, the change_type is something we don't know, so skip
				else
				{
					printf("ERROR: Change file has an unknown change type.  Skipping.\n");
					std::stringstream sstr;
					sstr << "ERROR: Change file has an unknown change type on line # " << change_line_num << ". Skipping this line.";
					write_to_log(sstr.str());
					//skip the rest of the while loop and get the next line in the change file.
					continue;
					//exit_program();
				}
			}
		}
		// If no matches found in change file, simply output the master file line to the temp file
		if (!match_found)
		{
			output_master_line(master_line, temp_file);
			total_copies++;
		}
		
	}
	fclose(master_file);
	fclose(change_file);
	fclose(temp_file);

	std::stringstream sstr;
	sstr << "Processed " << total_changes << " changes, "  << total_deletes << " deletes and copied over " << total_copies << " lines from master file unchanged";
	write_to_log(sstr.str());

	return 0;
}


// process_additions() - This function applies all additions described in the change file, applying
// them to the temporary output file.
// The approach is to simply walk through the change file one line at a time, looking for "ADD" directives.
// For each, append the line without modification to the end of the temporary output file.
// Note that we don't check that an ADD will cause a redundant (conflicting) record in the output file 
// because that's checked in process_changes_and_deletes().
// If any issues are found, we exit the program with an error message.  Otherwise, 0 is returned.
//
int process_additions()
{
	int total_additions = 0;

	char master_line[MAX_LINE_LENGTH];
	char master_unique_id[256];
	char change_line[MAX_LINE_LENGTH];
	char change_unique_id[256];
	char change_type[256];
	bool match_found;

	// Open Change and Temp files
	FILE* change_file;
	change_file = fopen(ChangeFileSourceFullName, "r");
	FILE* temp_file;
	temp_file = fopen(TempFileFullName, "a");
	FILE* master_file;
	master_file = fopen(MasterFileFullName, "r");

	if (master_file == NULL)
	{
		printf("ERROR: Cannot open Master File (%s) for reading.  Exiting.\n", MasterFileFullName);
		write_to_log("ERROR: Cannot open Master File for reading.  Exiting.");
		exit_program();
	}

	if (change_file == NULL)
	{
		printf("ERROR: Cannot open Change File (%s) for reading.  Exiting.\n", ChangeFileSourceFullName);
		write_to_log("ERROR: Cannot open Change File for reading.  Exiting.");
		exit_program();
	}
	if (temp_file == NULL)
	{
		printf("ERROR: Cannot open Temporary file (%s) for appending.  Exiting.\n", TempFileFullName);
		write_to_log("ERROR: Cannot open Temporary file for appending.  Exiting.");
		
		exit_program();
	}

	// Read in Change file one line at a time and look for lines with "ADD" directive
	fgets(change_line, MAX_LINE_LENGTH, change_file);  // Get first line (header) and throw it away
	int change_line_num = 1;
	while (fgets(change_line, MAX_LINE_LENGTH, change_file))
	{
		change_line_num++;
		match_found = false;

		if (get_change_type(change_line, change_type) != 0)
		{
			printf("ERROR: Could not find change type for line in Change File.\n");
			std::stringstream sstr;
			sstr << "ERROR: Could not find change type for line # " << change_line_num << " in Change File. Skipping record.";
			write_to_log(sstr.str());
			//skip the rest of the while loop and get the next line in the change file.
			continue;
			//exit_program();
		}


		if (get_unique_id_in_change_file(change_line, change_unique_id) != 0)
		{
			printf("ERROR: Could not find unique ID for line in Change File.  Exiting.\n");
			std::stringstream sstr;
			sstr << "ERROR: Could not find unique ID for line # " << change_line_num << " in Change File. Skipping record.";
			write_to_log(sstr.str());
			//skip the rest of the while loop and get the next line in the change file.
			continue;
		}

		// If it's an ADD, search the Master file for a duplicate
		// If it's a CHG, make sure there is a matching record in Master, otherwise copy it over
		if (strcmp(change_type, "\"ADD\"") == 0 || strcmp(change_type, "\"CHG\"") == 0 || strcmp(change_type, "\"C2E\"") == 0)
		{
			fseek(master_file, 0, SEEK_SET);
			fgets(master_line, MAX_LINE_LENGTH, master_file);  // Get first line (header) and throw it out
			int master_line_num = 1;
			while (fgets(master_line, MAX_LINE_LENGTH, master_file))
			{
				master_line_num++;
				if (get_unique_id_in_master_file(master_line, master_unique_id) != 0)
				{
					//Don't print a message as it's redundant from the process_changes_and_deletes() method
					//printf("ERROR: Could not find unique ID for line in Master File.  Skipping.\n");
					//std::stringstream sstr;
					//sstr << "ERROR: Could not find unique ID " << master_unique_id << " for line # " << master_line_num << " in Master File. Skipping this line.";
					//write_to_log(sstr.str());
					//Skip the rest of the while loop and get the next line in the master file.
					continue;
				}						

				if (strcmp(change_type, "\"ADD\"") == 0 && strcmp(change_unique_id, master_unique_id) == 0)
				{
					match_found = true;
					std::stringstream sstr;
					sstr << "ERROR: Skipping ADD operation on line # " << change_line_num << " of the change file because a matching record already exists in the Master file.";
					write_to_log(sstr.str());
					break;
				}
				else if ((strcmp(change_type, "\"CHG\"") == 0 || strcmp(change_type, "\"C2E\"") == 0) && strcmp(change_unique_id, master_unique_id) == 0)
				{
					match_found = true;
					break;
				}
			}
			//If there is no duplicate in the Master file, append the change record to the output file
			if (!match_found) {
				output_change_line(change_line, temp_file);
				total_additions++;
			}
		}
	}
	fclose(change_file);
	fclose(temp_file);
	fclose(master_file);

	std::stringstream sstr;
	sstr << "Processed " << total_additions << " additions";
	write_to_log(sstr.str());

	return 0;
}


//go to the directory containing the change files and look for folders matching the date range in the cfg
//if the date range is 'today', look for a folder with today's date
int get_file_name()
{
	//assume there is a / at the end of SourcePath, because when the config was read in it made sure there was a / at the end

	//remove trailing line break
	FolderDate.erase(FolderDate.length()-1);

	//if it's 8 chars long, assume it's in mmddyyyy format
	if (FolderDate.length() == 8) {

		std::stringstream sstr;
		sstr << SourcePath << FolderDate << '\\' << ChangeFileName;
		strcpy(ChangeFileSourceFullName, sstr.str().c_str());
	}
	
	//otherwise, just use today's date
	else {
		SYSTEMTIME today;
		GetLocalTime(&today);
		int day = today.wDay;
		int month = today.wMonth;
		int year = today.wYear;

		std::stringstream sstr;
		sstr << SourcePath;

		if (month < 10)
			sstr << 0;
		sstr << month;

		if (day < 10)
			sstr << 0;
		sstr << day;

		sstr << year << '\\' << ChangeFileName; 

		//cout << sstr.str() << endl;
		strcpy(ChangeFileSourceFullName, sstr.str().c_str());
	}

	std::stringstream sstr;
	sstr << "Loading change file '";
	sstr << ChangeFileSourceFullName;
	sstr << "'";
	write_to_log(sstr.str());

   return 0;
}



// cleanup_files() - Cleans up after the file processing is complete.
// This involves three steps:
//   - Archiving the change file
//   - Archiving the existing master file
//   - Renaming the temporary output file to be the new Master file
// This is made a bit painful by the annoying API for these operations in Windows.
// One of the big issues is making sure the file paths are conformant (e.g. \\).
//
int cleanup_files()
{
	char source_file_path [256];
	char dest_file_path [256];
	char time_stamp [256];

	// Get the current timestamp and format it
	time_t raw_time;
	time (&raw_time);
	strcpy(time_stamp, ctime(&raw_time));
	for (int i = 0; i < (int)strlen(time_stamp); i++)
	{
		if (time_stamp[i] == ' ')
			time_stamp[i] = '_';
		if (time_stamp[i] == ':')
			time_stamp[i] = '-';
		if (time_stamp[i] == '\n')
			time_stamp[i] = '\0';
	}

	// Create the archive directory (will have no effect if it already exists)
	_mkdir(ArchivePath);

	// Move or copy the change file to the archive directory
	strcpy(source_file_path, ChangeFileSourceFullName);
	scrub_filename(source_file_path);
	strcpy(dest_file_path, ArchivePath);
	strcat(dest_file_path, "change_");
	strcat(dest_file_path, time_stamp);
	scrub_filename(dest_file_path);

	if (ChangeFileArchive.find("move")==0) {
		if (rename(source_file_path, dest_file_path) != 0)
		{
			printf("ERROR: Cannot move change file (%s) to archive.  Exiting.\n", source_file_path);
			write_to_log("ERROR: Cannot move change file to archive.  Exiting.");
			exit_program();
		}

		write_to_log("Change file moved to archive folder");
	}
	else {
		ifstream f1(ChangeFileSourceFullName, fstream::binary);
		ofstream f2(dest_file_path, fstream::trunc|fstream::binary);
		f2 << f1.rdbuf();

		write_to_log("Change file copied to archive folder");
	}

	// Copy the old master file to the archive directory
	strcpy(source_file_path, MasterFileFullName);
	scrub_filename(source_file_path);
	strcpy(dest_file_path, ArchivePath);
	strcat(dest_file_path, "old_master_");
	strcat(dest_file_path, time_stamp);
	scrub_filename(dest_file_path);
	if (rename(source_file_path, dest_file_path) != 0)
	{
		printf("ERROR: Cannot move old master file (%s) to archive.  Exiting.\n", source_file_path);
		write_to_log("ERROR: Cannot move old master file to archive.  Exiting.");
		exit_program();
	}
	else
		write_to_log("Old master file copied to the archive folder");

	// Rename the temporary output file to be the new Master file
	strcpy(source_file_path, TempFileFullName);
	scrub_filename(source_file_path);
	strcpy(dest_file_path, MasterFileFullName);
	scrub_filename(dest_file_path);
	if (rename(source_file_path, dest_file_path) != 0)
	{
		printf("ERROR: Cannot move temporary file (%s) to Master.  Exiting.\n", source_file_path);
		write_to_log("ERROR: Cannot move temporary file to Master.  Exiting.");
		exit_program();
	}

	write_to_log("File Merge complete");
	write_to_log("------------------------------------------------");

	//close the log file
	fclose(log_file);

	return 0;
}


int open_log()
{

	log_file = fopen(LOG_FILE_NAME, "a");
	write_to_log("------------------------------------------------");
	write_to_log("Starting File Merge");
	return 0;
}




int _tmain(int argc, _TCHAR* argv[])
{
	int result = 0;
	result = open_log();
	result = load_config();
	result = get_file_name();
	result = process_changes_and_deletes();
	result = process_additions();
	result = cleanup_files();
	return 0;
}

