//#include <iostream>
#include <cstdlib>
#include <sstream>
// #include <fstream>
// #include <cmath>
#include <cstring>
#include <popt.h>
#include <regex>
#include <string.h>
// #include <time.h>

// Required for rand()
#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */

// #include <BamMultiReader.h>
#include "api/BamReader.h"
#include "api/BamWriter.h"

using namespace std;
using namespace BamTools;


bool isSameCigar(vector<CigarOp> v1, vector<CigarOp> v2)
{
    if (v1.size() != v2.size())
        return false;

    for (int i = 0; i < v1.size(); i++)
    {
        if (v1[i].Type != v2[i].Type)
            return false;
        if (v1[i].Length != v2[i].Length)
            return false;
    }
    return true;
}

bool parseHeader(string textHeader,
                 string &headerHD,
                 vector<string> &headerSQ,
                 vector<string> &headerRG,
                 vector<string> &headerPG,
                 vector<string> &headerCO)
{
    string line;
    istringstream iss(textHeader);
    while (getline(iss, line))
    {
        if (strncmp(line.c_str(), "@HD", 3) == 0)
        {
            headerHD += line;
        }
        else if (strncmp(line.c_str(), "@SQ", 3) == 0)
        {
            headerSQ.push_back(line);
        }
        else if (strncmp(line.c_str(), "@RG", 3) == 0)
        {
            headerRG.push_back(line);
        }
        else if (strncmp(line.c_str(), "@PG", 3) == 0)
        {
            headerPG.push_back(line);
        }
        else if (strncmp(line.c_str(), "@CO", 3) == 0)
        {
            headerCO.push_back(line);
        }
        else
        {
            cerr << "Error: Unknown header tag." << endl;
            return false;
        }
    }
    return true;
}

string random_string(size_t length)
{
    auto randchar = []() -> char {
        const char charset[] = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };
    string str(length, 0);
    generate_n(str.begin(), length, randchar);
    return str;
}

int main(int argc, const char *argv[])
{
    char *trashFileName = nullptr;
    char *logFileName = nullptr;
    char *ref1Name = nullptr;
    char *ref2Name = nullptr;

    /* initialize random seed: */
    srand(time(NULL));

    // clang-format off
    struct poptOption optionsTable[] = {
        {"trashfile", 't', POPT_ARG_STRING, &trashFileName, 0, "Set name of file collecting unmapped and other undesirable alignments", "path/name"},
        {"trashfile", 'T', POPT_ARG_NONE, 0, 0, "Generate file collecting unmapped and other undesirable alignments in " "outfilePATH/outfileNAME.trash", NULL},
        {"logfile", 'l', POPT_ARG_STRING, &logFileName, 0, "Set log file name", "path/name"},
        {"refname1", 'a', POPT_ARG_STRING, &ref1Name, 0, "Set first reference name", "name"},
        {"refname2", 'b', POPT_ARG_STRING, &ref2Name, 0, "Set second reference name", "name"},
        POPT_AUTOHELP{NULL, 0, 0, NULL, 0}};
    // clang-format on

    poptContext optCon;
    optCon = poptGetContext("bam-mergeRef", argc, argv, optionsTable, 0);
    // poptSetOtherOptionHelp( optCon, "[OPTIONS]* <inputfile1> <inputfile2> <outputfile>
    // <trashfile>" ) ;
    poptSetOtherOptionHelp(optCon,
                           "[OPTIONS]* -a <reference name 1> -b <reference name 2> <inputfile1> "
                           "<inputfile2> <outputfile>");
    int rc = poptGetNextOpt(optCon);
    if (rc != -1)
    {
        poptPrintUsage(optCon, stderr, 0);
        return 1;
    }

    // inputfile present?
    const char *infile1 = poptGetArg(optCon);
    if (!infile1)
    {
        cerr << "Error: need inputfile 1 as argument." << endl;
        poptPrintUsage(optCon, stderr, 0);
        return 1;
    }

    const char *infile2 = poptGetArg(optCon);
    if (!infile2)
    {
        cerr << "Error: need inputfile 2 as argument." << endl;
        poptPrintUsage(optCon, stderr, 0);
        return 1;
    }

    const char *outfile = poptGetArg(optCon);
    if (!outfile)
    {
        cerr << "Error: need outputfile as argument." << endl;
        poptPrintUsage(optCon, stderr, 0);
        return 1;
    }

    if (ref1Name == nullptr || ref2Name == nullptr)
    {
        cerr << "Error: please provide names for the two references." << endl;
        poptPrintUsage(optCon, stderr, 0);
        return 1;
    }

    if (trashFileName == nullptr)
    {
        for (int i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], "-T") == 0)
            {
                trashFileName = (char *)malloc((size_t)strlen(outfile) + 6);
                strcpy(trashFileName, outfile);
                strcat(trashFileName, ".trash");
            }
        }
    }

    BamReader *mFile1 = new BamReader;   // Create reader
    BamReader *mFile2 = new BamReader;   // Create reader
    BamWriter *mOutFile = new BamWriter; // Create writer

    BamWriter *mTrashFile = nullptr;
    if (trashFileName != nullptr)
    {
        mTrashFile = new BamWriter; // Create writer
    }

    // Open infile 1
    if (!mFile1->Open(infile1))
    {
        cerr << "Error: Could not open inputfile 1." << endl;
        poptPrintUsage(optCon, stderr, 0);
        delete mFile1;
        delete mFile2;
        delete mOutFile;
        if (mTrashFile != nullptr)
            delete mTrashFile;
        return 1;
    }

    // Open infile 2
    if (!mFile2->Open(infile2))
    {
        cerr << "Error: Could not open inputfile 2." << endl;
        poptPrintUsage(optCon, stderr, 0);
        mFile1->Close();
        delete mFile1;
        delete mFile2;
        delete mOutFile;
        if (mTrashFile != nullptr)
            delete mTrashFile;
        return 1;
    }

    // Both input files successfully opened
    string textHeader1 = mFile1->GetHeader().ToString();
    string textHeader2 = mFile2->GetHeader().ToString();
    string textHeaderOut;

    string line;
    string headerHD1;
    vector<string> headerSQ1;
    vector<string> headerRG1;
    vector<string> headerPG1;
    vector<string> headerCO1;

    string headerHD2;
    vector<string> headerSQ2;
    vector<string> headerRG2;
    vector<string> headerPG2;
    vector<string> headerCO2;

    string headerHDout;
    vector<string> headerSQout;
    vector<string> headerRGout;
    vector<string> headerPGout;
    vector<string> headerCOout;

    if (!parseHeader(textHeader1, headerHD1, headerSQ1, headerRG1, headerPG1, headerCO1))
    {
        cerr << "Error: Unknown header tag." << endl;
        mFile1->Close();
        mFile2->Close();
        delete mFile1;
        delete mFile2;
        if (mTrashFile != nullptr)
            delete mTrashFile;
        return 1;
    }

    if (!parseHeader(textHeader2, headerHD2, headerSQ2, headerRG2, headerPG2, headerCO2))
    {
        cerr << "Error: Unknown header tag." << endl;
        mFile1->Close();
        mFile2->Close();
        delete mFile1;
        delete mFile2;
        delete mOutFile;
        if (mTrashFile != nullptr)
            delete mTrashFile;
        return 1;
    }

    if (headerHD1.compare(headerHD2) == 0)
    {
        headerHDout = headerHD1;
    }
    else
    {
        cerr << "Error: The header lines (@HD) are different." << endl;
        mFile1->Close();
        mFile2->Close();
        delete mFile1;
        delete mFile2;
        delete mOutFile;
        if (mTrashFile != nullptr)
            delete mTrashFile;
        return 1;
    }

    // Merge @SQ lines
    // Should do a function void mergeSQ(header1, header2, header3)
    int i = 0, j = 0;
    regex ANregex(
        "\tAN:[0-9A-Za-z][0-9A-Za-z\\*\\+\\.@_\\|-]*(,[0-9A-Za-z][0-9A-Za-z\\*\\+\\.@_\\|-]*)*");
    smatch matchAN;

    regex SNregex("\tSN:([0-9A-Za-z][0-9A-Za-z\\*\\+\\.@_\\|-]*)");
    smatch matchSN;

    while (i < headerSQ1.size() && j < headerSQ2.size())
    {
        // cout << "while1\n";
        // cout << headerSQ1[i] << headerSQ2[j] << "\n";
        if (strverscmp(headerSQ1[i].c_str(), headerSQ2[j].c_str()) == 0)
        {
            if (!regex_search(headerSQ1[i], matchSN, SNregex))
            {
                cerr << "Error: A header line (@SQ) is missing its SN tag in both input files."
                     << endl;
                mFile1->Close();
                mFile2->Close();
                delete mFile1;
                delete mFile2;
                delete mOutFile;
                if (mTrashFile != nullptr)
                    delete mTrashFile;
                return 1;
            }
            // cout << matchSN[0] << "\t" << matchSN[1] << "\n";

            string str;
            if (regex_search(headerSQ1[i], matchAN, ANregex)) // GET SN TO ADD BEFORE REFERENCE NAME
            {
                str += matchAN.prefix();
                str += matchAN[0];
                str += ",";
            }
            else
            {
                str += headerSQ1[i];
                str += "\tAN:";
                // cout << matchSN[0] << "\t" << matchSN[1] << "\n";
            }
            str += matchSN[1];
            str += "-";
            str += ref1Name;
            str += "-1,";
            str += matchSN[1];
            str += "-";
            str += ref2Name;
            str += "-2";
            str += matchAN.suffix();
            // cout << str << "\n";
            headerSQout.push_back(str);
            i++;
            j++;
        }
        else if (strverscmp(headerSQ1[i].c_str(), headerSQ2[j].c_str()) < 0)
        {
            if (!regex_search(headerSQ1[i], matchSN, SNregex))
            {
                cerr << "Error: A header line (@SQ) is missing its SN tag in input file 1." << endl;
                mFile1->Close();
                mFile2->Close();
                delete mFile1;
                delete mFile2;
                delete mOutFile;
                if (mTrashFile != nullptr)
                    delete mTrashFile;
                return 1;
            }

            string str;
            if (regex_search(headerSQ1[i], matchAN, ANregex)) // GET SN TO ADD BEFORE REFERENCE NAME
            {
                str += matchAN.prefix();
                str += matchAN[0];
                str += ",";
            }
            else
            {
                str += headerSQ1[i];
                str += "\tAN:";
            }
            str += matchSN[1];
            str += "-";
            str += ref1Name;
            str += "-1"; // ADD PARENTHESES AROUND REFNAME
            str += matchAN.suffix();
            headerSQout.push_back(str);
            i++;
        }
        else
        {
            if (!regex_search(headerSQ2[j], matchSN, SNregex))
            {
                cerr << "Error: A header line (@SQ) is missing its SN tag in input file 2." << endl;
                mFile1->Close();
                mFile2->Close();
                delete mFile1;
                delete mFile2;
                delete mOutFile;
                if (mTrashFile != nullptr)
                    delete mTrashFile;
                return 1;
            }
            string str;
            if (regex_search(headerSQ2[j], matchAN, ANregex)) // GET SN TO ADD BEFORE REFERENCE NAME
            {
                str += matchAN.prefix();
                str += matchAN[0];
                str += ",";
            }
            else
            {
                str += headerSQ2[j];
                str += "\tAN:";
            }
            str += matchSN[1];
            str += "-";
            str += ref2Name;
            str += "-2"; // ADD PARENTHESES AROUND REFNAME
            str += matchAN.suffix();
            headerSQout.push_back(str);
            j++;
        }
    }
    while (i < headerSQ1.size())
    {
        // cout << "while2\n";
        // cout << headerSQ1[i] << "\n";
        if (!regex_search(headerSQ1[i], matchSN, SNregex))
        {
            cerr << "Error: A header line (@SQ) is missing its SN tag in input file 1." << endl;
            mFile1->Close();
            mFile2->Close();
            delete mFile1;
            delete mFile2;
            delete mOutFile;
            if (mTrashFile != nullptr)
                delete mTrashFile;
            return 1;
        }
        string str;
        if (regex_search(headerSQ1[i], matchAN, ANregex)) // GET SN TO ADD BEFORE REFERENCE NAME
        {
            str += matchAN.prefix();
            str += matchAN[0];
            str += ",";
        }
        else
        {
            str += headerSQ1[i];
            str += "\tAN:";
        }
        str += matchSN[1];
        str += "-";
        str += ref1Name;
        str += "-1"; // ADD PARENTHESES AROUND REFNAME
        str += matchAN.suffix();
        headerSQout.push_back(str);
        i++;
    }
    while (j < headerSQ2.size())
    {
        // cout << "while3\n";
        // cout << headerSQ2[j] << "\n";
        if (!regex_search(headerSQ2[j], matchSN, SNregex))
        {
            cerr << "Error: A header line (@SQ) is missing its SN tag in input file 2." << endl;
            mFile1->Close();
            mFile2->Close();
            delete mFile1;
            delete mFile2;
            delete mOutFile;
            if (mTrashFile != nullptr)
                delete mTrashFile;
            return 1;
        }
        string str;
        if (regex_search(headerSQ2[j], matchAN, ANregex)) // GET SN TO ADD BEFORE REFERENCE NAME
        {
            str += matchAN.prefix();
            str += matchAN[0];
            str += ",";
        }
        else
        {
            str += headerSQ2[j];
            str += "\tAN:";
        }
        str += matchSN[1];
        str += "-";
        str += ref2Name;
        str += "-2"; // ADD PARENTHESES AROUND REFNAME
        str += matchAN.suffix();
        headerSQout.push_back(str);
        j++;
    }

    // Merge @RG lines
    // concatenate vectors into 1
    headerRGout.reserve(headerRG1.size() + headerRG2.size()); // preallocate memory
    headerRGout.insert(headerRGout.end(), headerRG1.begin(), headerRG1.end());
    headerRGout.insert(headerRGout.end(), headerRG2.begin(), headerRG2.end());

    // sort vectors
    sort(headerRGout.begin(), headerRGout.end());
    // use unique for removing consecutive identical elements //see how to do here:
    // http://www.cplusplus.com/reference/algorithm/unique/
    vector<string>::iterator it;
    it = unique(headerRGout.begin(), headerRGout.end());
    headerRGout.resize(distance(headerRGout.begin(), it));

    // Update @PG lines

    // Regular expression to parse previous ID
    regex IDregex("\tID:([0-9A-Za-z][0-9A-Za-z\\*\\+\\.@_\\|-]*)");
    smatch matchID1;
    smatch matchID2;

    string programID;
    programID = "\tID:bam-mergeRef";
    bool previousRun;
    previousRun = false;

    regex PPregex("\tPP:([0-9A-Za-z][0-9A-Za-z\\*\\+\\.@_\\|-]*)");
    smatch matchPP;

    string previousProgram;
    bool updatePP;
    updatePP = false;
    string newPP;

    for (int i = headerPG1.size() - 1; i >= 0; i--)
    {
        string str;
        string ID;
        if (updatePP)
        {
            if (regex_search(headerPG1[i], matchPP, PPregex))
                str = matchPP.prefix();
            else
                str = headerPG1[i];
            str += "\tPP:";
            // cout << newPP << "\n";
            str += newPP;
            str += matchPP.suffix();
            headerPG1[i] = str;
            // cout << "changed to:" << headerPG1[i] << "\n";
            updatePP = false;
        }
        // cout << headerPG1[i] << "\n";
        regex_search(headerPG1[i], matchID1, IDregex); // ID SHOULD ALWAYS PRESENT IN A @PG LINE
        // cout << headerPG1[i] << "\n";
        for (int j = 0; j < headerPG2.size(); j++)
        {
            regex_search(headerPG2[j], matchID2, IDregex); // ID SHOULD ALWAYS PRESENT IN A @PG LINE
            if (programID == matchID1[1] || programID == matchID2[1])
                previousRun = true;
            if (matchID1[1] == matchID2[1])
            {
                // cout << matchID1[1] << "\t" << matchID2[1] << "\n";
                str = matchID1.prefix();
                str += matchID1[0];
                str += "-";
                ID = random_string(8);
                // cout << ID << "\n";
                str += ID;
                str += matchID1.suffix();
                // cout << str << "\n";
                updatePP = true;
                newPP = matchID1[1];
                newPP += "-";
                newPP += ID;
                // cout << "newPP:" << newPP << "\n";
                headerPG1[i] = str;
            }
        }
        // cout << headerPG1[i] << "\n";
    }

    string newPG;
    newPG = "@PG";
    newPG += programID;
    if (previousRun)
    {
        newPG += "-";
        newPG += random_string(8);
    }
    newPG += "\tPN:bam-mergeRef\tPP:";
    regex_search(headerPG1[0], matchID1, IDregex);
    newPG += matchID1[1];
    newPG += "\tCL:";
    newPG += argv[0];
    for (int i = 1; i < argc; i++)
    {
        newPG += " ";
        newPG += argv[i];
    }
    // headerPGout.reserve( headerPG1.size() + headerPG2.size() + 1);
    // headerPGout.insert( headerPGout.end(), headerPG1.begin(), headerPG1.end() );
    // OLDEST @PG SHOULD BE THE LAST AND NEWEST @PG SHOULD BE THE FIRST

    // CONCATENATE ALL THE VECTOR IN mHeaderOut
    textHeaderOut += headerHDout;
    textHeaderOut += "\n";
    for (auto str : headerSQout)
        textHeaderOut += str + "\n";
    for (auto str : headerRGout)
        textHeaderOut += str + "\n";
    textHeaderOut += newPG + "\n";
    for (auto str : headerPG1)
        textHeaderOut += str + "\n";
    for (auto str : headerPG2)
        textHeaderOut += str + "\n";
    for (auto str : headerCO1)
        textHeaderOut += str + "\n";
    for (auto str : headerCO2)
        textHeaderOut += str + "\n";


    // Open output file
    if (!mOutFile->Open(outfile,
                        SamHeader(textHeaderOut),
                        mFile1->GetReferenceData())) // CHANGE WHICH HEADER IS WRITTEN TO mHeaderOut
    {
        cerr << "Error: Could not write outputfile." << endl;
        poptPrintUsage(optCon, stderr, 0);
        mFile1->Close();
        mFile2->Close();
        delete mFile1;
        delete mFile2;
        delete mOutFile;
        if (mTrashFile != nullptr)
            delete mTrashFile;
        return 1;
    }
    mOutFile->SetCompressionMode(BamWriter::Compressed);

    if (mTrashFile != nullptr)
    {
        if (!mTrashFile->Open(trashFileName,
                              SamHeader(textHeaderOut),
                              mFile1->GetReferenceData())) // MAKE THIS OPTIONAL ! + ADD A SUFFIX
        {
            cerr << "Error: Could not write trashfile." << endl;
            poptPrintUsage(optCon, stderr, 0);
            mFile1->Close();
            mFile2->Close();
            mOutFile->Close();
            delete mFile1;
            delete mFile2;
            delete mOutFile;
            delete mTrashFile;
            return 1;
        }
        mTrashFile->SetCompressionMode(BamWriter::Compressed);
    }

    // Ready to process

    string previousAlnName = "0"; // Check if '0' is first character
    // If string doesn't work try with : char prevName[20];

    char error = 0;

    // While condition
    bool readLine1 = false; // readLine1 == false -> a new line should be read
    bool readLine2 = false;
    BamAlignment aln1;
    BamAlignment aln2;

    while (1) // Read all file lines until end of file
    {
        if (!readLine1)
            readLine1 = mFile1->GetNextAlignment(aln1);

        if (!readLine2)
            readLine2 = mFile2->GetNextAlignment(aln2);

        if (!readLine1 && !readLine2) // If both files are empty exit loop
        {
            // Check if corrupted file or normal : end of file ?
            break; // Exit while
        }

        // Algorithm
        //		if(aln1.Name!=aln2.Name) // If not the same name
        //		{
        // cerr << "Error: The entries of both BAM files are not identical. Either you do not have
        // the same sequences in both files or you did not sort the entries by names." << endl;
        // poptPrintUsage( optCon, stderr, 0 ) ;
        // error = 1;
        // break;
        //		cout << aln1.Name << "\t" << aln2.Name << "\n";
        if (aln1.Name != aln2.Name)
        {
            BamAlignment *aln;
            BamReader *mFile;
            int currentFileNumber;

            // cout << aln1.Name << "\t" << aln2.Name << "\n";

            if (!readLine1)
                cout << "EOF File1\n";
            if (!readLine2)
                cout << "EOF File2\n";

            if ((readLine1) // If lines in File 1
                && ((strverscmp(aln1.Name.c_str(), aln2.Name.c_str()) < 0)
                    || readLine2 == false)) // And name1 < name2 or file 2 empty
            {
                // cout << "Fichier 1\n";
                aln = &aln1;
                mFile = (BamReader *)mFile1;
                currentFileNumber = 1;
                readLine1 = false; // readLine1 is treated, next loop should read a new one
            }
            else // Else : file 1 empty or name2 < name1 => parse file 2
            {
                // cout << "Fichier 2\n";
                aln = &aln2;
                mFile = (BamReader *)mFile2;
                currentFileNumber = 2;
                readLine2 = false; // readLine2 is treated, next loop should read a new one
            }

            if (strverscmp(aln->Name.c_str(), previousAlnName.c_str())
                <= 0) // from string.h // If not sorted // verify that it is the right order (maybe
                      // change to <0)
            {
                // Data are not sorted get out
                // Or maybe pair that was not previously detected with IsPaired() ???
                cerr << "Error: Please sort the entries of your BAM files by names. 3" << endl;
                cerr << aln->Name << "\t" << previousAlnName << endl;
                poptPrintUsage(optCon, stderr, 0);
                error = 1;
                break;
            }
            previousAlnName = aln->Name; // Update previous name

            aln->AddTag("RN", "i", currentFileNumber);

            bool mapped = aln->IsMapped();
            BamAlignment alnNext;

            if (aln->IsPaired())
            {
                mFile->GetNextAlignment(alnNext);
                alnNext.AddTag("RN", "i", currentFileNumber);
                if (aln->Name != alnNext.Name)
                {
                    cerr << "Error: A widow was encountered in file " << currentFileNumber
                         << ". Check that all paired reads have a mate or sort your BAM files by "
                            "names"
                         << endl;
                    poptPrintUsage(optCon, stderr, 0);
                    error = 1;
                    break;
                }
                mapped = aln->IsMapped() || alnNext.IsMapped();
            }

            if (mapped)
            {
                mOutFile->SaveAlignment(*aln);
                if (aln->IsPaired())
                    mOutFile->SaveAlignment(alnNext);
            }
            else
            {
                if (mTrashFile != nullptr)
                {
                    mTrashFile->SaveAlignment(*aln);
                    if (aln->IsPaired())
                        mTrashFile->SaveAlignment(alnNext);
                }
            }
            // cout << aln->Name << "\n";
        }
        else // both files are treated simultaneously
        {
            // cout << "Fichier 1 et 2\n";
            readLine1 = false; // readLine1 is computed next loop should read a new one
            readLine2 = false; // readLine2 is computed next loop should read a new one
            if (strverscmp(aln1.Name.c_str(), previousAlnName.c_str())
                <= 0) //(aln1.Name<=previousAlnName)
                      ////(strverscmp(aln1.Name.c_str(),previousAlnName.c_str())<=0) from string.h
                      //// If not sorted // verify that it is the right order (maybe change to <0)
            {
                // Data are not sorted get out
                // Or maybe pair that was not previously detected with IsPaired() ???
                cerr << "Error: Please sort the entries of your BAM files by names. 3" << endl;
                cerr << aln1.Name << "\t" << previousAlnName << endl;
                poptPrintUsage(optCon, stderr, 0);
                error = 1;
                break;
            }
            previousAlnName = aln1.Name; // Update previous name

            // If paired
            if (aln1.IsPaired() && aln2.IsPaired())
            {
                // Algorithm pair
                // Load second mate
                BamAlignment aln3;
                BamAlignment aln4;
                if (!mFile1->GetNextAlignment(aln3) || !mFile2->GetNextAlignment(aln4))
                {
                    cerr << "Error: Reached the end of the file (or could not read the next entry) "
                            "without finding a mate. Check that all paired reads have a mate"
                         << endl; // SHOULD BE ABLE TO DIFFERENTIATE BETWEEN END OF FILE AND A WRONG
                                  // LINE...
                    poptPrintUsage(optCon, stderr, 0);
                    error = 1;
                    break;
                }

                if (aln1.Name != aln3.Name)
                {
                    cout << "Warning : Missing mate of " << aln1.Name << " in File 1\n";
                    readLine1 = true; // aln1 = aln3
                }
                if (aln2.Name != aln4.Name)
                {
                    cout << "Warning : Missing mate " << aln2.Name << " in File 2\n";
                    readLine2 = true; // aln2 = aln4
                }

                /*if((aln1.Name != aln3.Name) || (aln2.Name != aln4.Name))
                {
                        cout << aln1.Name << "\t" << aln3.Name << "\t" << aln2.Name << "\t" <<
                aln4.Name << "\n"; cerr << "Error: A widow was encountered. Check that all paired
                reads have a mate or sort your BAM files by names" << endl; poptPrintUsage( optCon,
                stderr, 0 ) ; error = 1; break;
                }*/

                // Update previousAlnName is not required because aln3/aln1 and aln4/aln2 have the
                // same name Algorithm here
                BamAlignment *alnKeep1;
                BamAlignment *alnKeep2;

                if (readLine1 && readLine2) // aln3 and aln4 are not mates
                {
                    BamAlignment *alnKeep;
                    if (!aln1.IsMapped())
                    {
                        if (!aln2.IsMapped())
                        {
                            if (mTrashFile != nullptr)
                            {
                                mTrashFile->SaveAlignment(aln1);
                            }
                        }
                        else // aln2 is mapped
                        {
                            alnKeep = &aln2;
                            alnKeep->AddTag("RN", "i", 2); // add tag that only aln2 was mapped
                        }
                    }
                    else // aln1 is mapped
                    {
                        if (!aln2.IsMapped())
                        {
                            alnKeep = &aln1;
                            alnKeep->AddTag("RN", "i", 1); // add tag that only aln1 was mapped
                        }
                        else // they are both mapped
                        {
                            if (aln1.Position != aln2.Position
                                || !isSameCigar(aln1.CigarData, aln2.CigarData))
                            {
                                if (mTrashFile != nullptr)
                                {
                                    // cout << aln1.Name << "\t" << aln1.Position << "\t" <<
                                    // aln2.Name << "\t" << aln2.Position << "\n";
                                    aln1.AddTag("RN", "i", 1);
                                    aln1.SetIsPrimaryAlignment(false);
                                    aln2.AddTag("RN", "i", 2);
                                    aln2.SetIsPrimaryAlignment(false);
                                    mTrashFile->SaveAlignment(aln1);
                                    mTrashFile->SaveAlignment(aln2);
                                }
                                aln1 = aln3;
                                aln2 = aln4;
                                continue;
                            }
                            // Random choice
                            if (rand() % 2 == 0)
                            {
                                alnKeep = &aln1; // Keep aln1
                            }
                            else
                            {
                                alnKeep = &aln2;
                            }
                            alnKeep->AddTag(
                                "RN", "i", 12); // add tag that both aln1 and aln2 were mapped
                        }
                    }
                    // Write aln to output file
                    // cout << "Here" << "\n";
                    mOutFile->SaveAlignment(*alnKeep);
                }
                else if (readLine1) // only aln3 is not a mate
                {
                    /*if(aln1.IsMapped() && !aln2.IsMapped())
                    {
                            // Deal with aln1 and aln4
                    }
                    if(!aln1.IsMapped() && aln2.IsMapped())
                    {
                            // write aln2 and aln4
                    }*/
                    if (mTrashFile != nullptr)
                    {
                        aln1.AddTag("RN", "i", 1);
                        aln2.AddTag("RN", "i", 2);
                        aln4.AddTag("RN", "i", 2);
                        // aln1.SetIsPrimaryAlignment(false); SHOULD I ADD THIS?
                        mTrashFile->SaveAlignment(aln1);
                        mTrashFile->SaveAlignment(aln2);
                        mTrashFile->SaveAlignment(aln4);
                    }
                }
                else if (readLine2) // only aln4 is not a mate
                {
                    if (mTrashFile != nullptr)
                    {
                        aln1.AddTag("RN", "i", 1);
                        aln3.AddTag("RN", "i", 1);
                        aln2.AddTag("RN", "i", 2);
                        mTrashFile->SaveAlignment(aln1);
                        mTrashFile->SaveAlignment(aln3);
                        mTrashFile->SaveAlignment(aln2);
                    }
                }
                else // if(!readLine1 && !readLine2) // aln3 and aln4 are mates
                {
                    if (!aln1.IsMapped() && !aln3.IsMapped())
                    {
                        if (aln2.IsMapped() || aln4.IsMapped())
                        {
                            alnKeep1 = &aln2;
                            alnKeep2 = &aln4;
                            alnKeep1->AddTag("RN", "i", 2);
                            alnKeep2->AddTag("RN", "i", 2);
                        }
                        else
                        {
                            if (mTrashFile != nullptr)
                            {
                                mTrashFile->SaveAlignment(aln1);
                                mTrashFile->SaveAlignment(aln3);
                            }
                            continue;
                        }
                    }
                    else
                    {
                        if (!aln2.IsMapped() && !aln4.IsMapped())
                        {
                            alnKeep1 = &aln1;
                            alnKeep2 = &aln3;
                            alnKeep1->AddTag("RN", "i", 1);
                            alnKeep2->AddTag("RN", "i", 1);
                        }
                        else
                        {
                            if ((aln1.IsFirstMate() && aln2.IsFirstMate())
                                || (!aln1.IsFirstMate() && !aln2.IsFirstMate()))
                            {
                                if (aln1.Position != aln2.Position
                                    || !isSameCigar(aln1.CigarData, aln2.CigarData)
                                    || aln3.Position != aln4.Position
                                    || !isSameCigar(aln3.CigarData, aln4.CigarData))
                                // I AM ASSUMING THAT THIS WORKS EVEN WHEN THE READ IS UNMAPPED,
                                // CHECK THAT!!
                                {
                                    if (mTrashFile != nullptr)
                                    {
                                        aln1.AddTag("RN", "i", 1);
                                        aln3.AddTag("RN", "i", 1);
                                        aln2.AddTag("RN", "i", 2);
                                        aln4.AddTag("RN", "i", 2);
                                        aln1.SetIsPrimaryAlignment(false);
                                        aln2.SetIsPrimaryAlignment(false);
                                        aln3.SetIsPrimaryAlignment(false);
                                        aln4.SetIsPrimaryAlignment(false);
                                        mTrashFile->SaveAlignment(aln1);
                                        mTrashFile->SaveAlignment(aln2);
                                        mTrashFile->SaveAlignment(aln3);
                                        mTrashFile->SaveAlignment(aln4);
                                    }
                                    continue;
                                }
                            }
                            else
                            {
                                if (aln1.Position != aln4.Position
                                    || !isSameCigar(aln1.CigarData, aln4.CigarData)
                                    || aln3.Position != aln2.Position
                                    || !isSameCigar(aln3.CigarData, aln2.CigarData))
                                {
                                    if (mTrashFile != nullptr)
                                    {
                                        aln1.AddTag("RN", "i", 1);
                                        aln3.AddTag("RN", "i", 1);
                                        aln2.AddTag("RN", "i", 2);
                                        aln4.AddTag("RN", "i", 2);
                                        aln1.SetIsPrimaryAlignment(false);
                                        aln2.SetIsPrimaryAlignment(false);
                                        aln3.SetIsPrimaryAlignment(false);
                                        aln4.SetIsPrimaryAlignment(false);
                                        mTrashFile->SaveAlignment(aln1);
                                        mTrashFile->SaveAlignment(aln2);
                                        mTrashFile->SaveAlignment(aln3);
                                        mTrashFile->SaveAlignment(aln4);
                                    }
                                    continue;
                                }
                            }
                            if (rand() % 2 == 0)
                            {
                                alnKeep1 = &aln1;
                                alnKeep2 = &aln3;
                            }
                            else
                            {
                                alnKeep1 = &aln2;
                                alnKeep2 = &aln4;
                            }
                            alnKeep1->AddTag("RN", "i", 12);
                            alnKeep2->AddTag("RN", "i", 12);
                        }
                    }
                    // Write aln 1 & 3 or 2 & 4
                    mOutFile->SaveAlignment(*alnKeep1);
                    mOutFile->SaveAlignment(*alnKeep2);
                }
                if (readLine1)
                    aln1 = aln3;
                if (readLine2)
                    aln2 = aln4;
            }
            // else if(aln1.IsPaired() || aln2.IsPaired()) // error in files ?
            else // not paired
            {
                BamAlignment *alnKeep;
                // Do algorithm
                if (!aln1.IsMapped())
                {
                    if (!aln2.IsMapped())
                    {
                        if (mTrashFile != nullptr)
                        {
                            mTrashFile->SaveAlignment(aln1);
                        }
                        continue; // go to next lines in the input files
                    }
                    else // aln2 is mapped
                    {
                        alnKeep = &aln2;
                        alnKeep->AddTag("RN", "i", 2); // add tag that only aln2 was mapped
                    }
                }
                else // aln1 is mapped
                {
                    if (!aln2.IsMapped())
                    {
                        alnKeep = &aln1;
                        alnKeep->AddTag("RN", "i", 1); // add tag that only aln1 was mapped
                    }
                    else // they are both mapped
                    {
                        if (aln1.Position != aln2.Position
                            || !isSameCigar(aln1.CigarData, aln2.CigarData))
                        {
                            if (mTrashFile != nullptr)
                            {
                                // cout << aln1.Name << "\t" << aln1.Position << "\t" << aln2.Name
                                // << "\t" << aln2.Position << "\n";
                                aln1.AddTag("RN", "i", 1);
                                aln1.SetIsPrimaryAlignment(false);
                                aln2.AddTag("RN", "i", 2);
                                aln2.SetIsPrimaryAlignment(false);
                                mTrashFile->SaveAlignment(aln1);
                                mTrashFile->SaveAlignment(aln2);
                            }
                            continue;
                        }
                        // Random choice
                        if (rand() % 2 == 0)
                        {
                            alnKeep = &aln1; // Keep aln1
                        }
                        else
                        {
                            alnKeep = &aln2;
                        }
                        alnKeep->AddTag(
                            "RN", "i", 12); // add tag that both aln1 and aln2 were mapped
                    }
                }

                // Write aln to output file
                // cout << "Here" << "\n";
                mOutFile->SaveAlignment(*alnKeep);
            }
        }
    }

    mFile1->Close(); // Close file
    mFile2->Close();
    mOutFile->Close();
    delete mFile1;
    delete mFile2;
    delete mOutFile;
    if (mTrashFile != nullptr)
    {
        mTrashFile->Close();
        delete mTrashFile;
    }
    return error;
}
