/*
 *  Copyright (C) 2011  Regents of the University of Michigan
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//////////////////////////////////////////////////////////////////////////
// This file contains the processing for the executable option "indelDiscordance"
// which generates some statistics for SAM/BAM files.
#include <math.h>
#include "IndelDiscordance.h"
#include "SamFile.h"
#include "BgzfFileType.h"
#include "Pileup.h"
#include "BaseAsciiMap.h"
#include "BaseUtilities.h"

const String IndelDiscordance::DEFAULT_UM_REF_LOC = "/data/local/ref/karma.ref/human.g1k.v37.umfa";
GenomeSequence* IndelDiscordance::PileupElementIndelDiscordance::ourReference = NULL;
int IndelDiscordance::PileupElementIndelDiscordance::ourMinDepth = 2;
bool IndelDiscordance::PileupElementIndelDiscordance::ourPrintPos = false;
RunningStat IndelDiscordance::PileupElementIndelDiscordance::ourRunningDepthStat;
uint32_t IndelDiscordance::PileupElementIndelDiscordance::ourTotalMinDepth = 0;
uint32_t IndelDiscordance::PileupElementIndelDiscordance::ourTotalDiscordant = 0;
uint32_t IndelDiscordance::PileupElementIndelDiscordance::ourTotalDelDiscordant = 0;
uint32_t IndelDiscordance::PileupElementIndelDiscordance::ourTotalInsDiscordant = 0;
std::map<uint32_t, IndelDiscordance::PileupElementIndelDiscordance::RepeatInfo> IndelDiscordance::PileupElementIndelDiscordance::ourRepeatInfo;

IndelDiscordance::IndelDiscordance()
    : BamExecutable()
{
}


void IndelDiscordance::indelDiscordanceDescription()
{
    std::cerr << " indelDiscordance - IndelDiscordance a SAM/BAM File" << std::endl;
}


void IndelDiscordance::description()
{
    indelDiscordanceDescription();
}


void IndelDiscordance::usage()
{
    BamExecutable::usage();
    std::cerr << "\t./bam indelDiscordance --in <inputFile> [--bamIndex <bamIndexFile] [--refFile <filename>] [--umRef] [--depth minDepth] [--minRepeatLen len] [--sumRepeatLen len] [--printPos] [--chrom <name>] [--start 0basedPos] [--end 0basedPos] [--noeof] [--params]" << std::endl;
    std::cerr << "\tRequired Parameters:" << std::endl;
    std::cerr << "\t\t--in : the SAM/BAM file to calculate indelDiscordance for" << std::endl;
    std::cerr << "\tOptional Parameters:" << std::endl;
    std::cerr << "\t\t--bamIndex     : The path/name of the bam index file" << std::endl;
    std::cerr << "\t\t                 (if required and not specified, uses the --in value + \".bai\")" << std::endl;
    std::cerr << "\t\t--refFile      : reference file for determining repeat counts" << std::endl;
    std::cerr << "\t\t--umRef        : use the reference at the default UofM location, " << std::endl
              << "\t\t                 " << DEFAULT_UM_REF_LOC << std::endl;
    std::cerr << "\t\t--depth        : min depth at which to report indel discordance, DEFAULT >= "
              << DEFAULT_MIN_DEPTH << std::endl;
    std::cerr << "\t\t--minRepeatLen : min repeat length for printing repeat info, DEFAULT = "
              << DEFAULT_MIN_REPEAT << std::endl;
    std::cerr << "\t\t--sumRepeatLen : all repeats this length and longer will be accumulated,\n"
              << "\t\t                 DEFAULT = " << DEFAULT_SUM_REPEAT << std::endl;
    std::cerr << "\t\t--avgDepthMult : max depth used is the average depth * this multiplier,\n"
              << "\t\t                 DEFAULT = " << DEFAULT_AVG_DEPTH_MULTIPLIER << std::endl;
    std::cerr << "\t\t--printPos     : print details for each position" << std::endl;
    std::cerr << "\t\t--printCounts  : print counts of occurrances of each repeat count and of discordant cigars for each repeat count" << std::endl;
    std::cerr << "\t\t--sample       : output the specified sample name as part of the error rate/depth table" << std::endl;
    std::cerr << "\t\t--gender       : output the specified gender as part of the error rate/depth table" << std::endl;
    std::cerr << "\t\t--chrom        : chromosome name other than X" << std::endl;
    std::cerr << "\t\t--start        : use a 0-based inclusive start position other than the default, "
              << DEFAULT_START_POS << std::endl;
    std::cerr << "\t\t--end          : use a 0-based exclusive end position other than the default, "
              << DEFAULT_END_POS << std::endl;
    std::cerr << "\t\t--noeof        : Do not expect an EOF block on a bam file." << std::endl;
    std::cerr << "\t\t--params       : Print the parameter settings" << std::endl;
    std::cerr << std::endl;
}


int IndelDiscordance::execute(int argc, char **argv)
{
    // Extract command line arguments.
    String inFile = "";
    String indexFile = "";
    String refFile = "";
    String chrom = "X";
    bool defaultUMRef = false;
    int minDepth = DEFAULT_MIN_DEPTH;
    int minRepeatLen = DEFAULT_MIN_REPEAT;
    int sumRepeatLen = DEFAULT_SUM_REPEAT;
    int avgDepthMultiplier = DEFAULT_AVG_DEPTH_MULTIPLIER;
    bool printPos = false;
    bool printCounts = false;
    String sample = "";
    String gender = "";
    int startPos0Based = DEFAULT_START_POS;
    int endPos0Based = DEFAULT_END_POS;
    bool noeof = false;
    bool params = false;

    ParameterList inputParameters;
    BEGIN_LONG_PARAMETERS(longParameterList)
        LONG_PARAMETER_GROUP("Required Parameters")
        LONG_STRINGPARAMETER("in", &inFile)
        LONG_PARAMETER_GROUP("Optional Parameters")
        LONG_STRINGPARAMETER("bamIndex", &indexFile)
        LONG_STRINGPARAMETER("refFile", &refFile)
        LONG_PARAMETER("umRef", &defaultUMRef)
        LONG_INTPARAMETER("depth", &minDepth)
        LONG_INTPARAMETER("minRepeatLen", &minRepeatLen)
        LONG_INTPARAMETER("sumRepeatLen", &sumRepeatLen)
        LONG_INTPARAMETER("avgDepthMult", &avgDepthMultiplier)
        LONG_PARAMETER("printPos", &printPos)
        LONG_PARAMETER("printCounts", &printCounts)
        LONG_STRINGPARAMETER("sample", &sample)
        LONG_STRINGPARAMETER("gender", &gender)
        LONG_STRINGPARAMETER("chrom", &chrom)
        LONG_INTPARAMETER("start", &startPos0Based)
        LONG_INTPARAMETER("end", &endPos0Based)
        LONG_PARAMETER("noeof", &noeof)
        LONG_PARAMETER("params", &params)
        END_LONG_PARAMETERS();
   
    inputParameters.Add(new LongParameters ("Input Parameters", 
                                            longParameterList));

    inputParameters.Read(argc-1, &(argv[1]));

    // If no eof block is required for a bgzf file, set the bgzf file type to 
    // not look for it.
    if(noeof)
    {
        // Set that the eof block is not required.
        BgzfFileType::setRequireEofBlock(false);
    }

    // Check to see if the in file was specified, if not, report an error.
    if(inFile == "")
    {
        usage();
        inputParameters.Status();
        // In file was not specified but it is mandatory.
        std::cerr << "--in is a mandatory argument for indelDiscordance, "
                  << "but was not specified" << std::endl;
        return(-1);
    }

    // Check to see if the IndexFile name has been set.
    if(indexFile == "")
    {
        // Index file was not specified, so set it to the input file + ".bai"
        indexFile = inFile + ".bai";
    }


    if(defaultUMRef)
    {
        refFile = DEFAULT_UM_REF_LOC;
    }

    ////////////////////////////////////////
    // Setup in case pileup is used.
    Pileup<PileupElementIndelDiscordance> pileup;
    
    if(params)
    {
        inputParameters.Status();
    }

    // Open the file for reading.
    SamFile samIn;
    if(!samIn.OpenForRead(inFile))
    {
        fprintf(stderr, "%s\n", samIn.GetStatusMessage());
        return(samIn.GetStatus());
    }

    // Read the sam header.
    SamFileHeader samHeader;
    if(!samIn.ReadHeader(samHeader))
    {
        fprintf(stderr, "%s\n", samIn.GetStatusMessage());
        return(samIn.GetStatus());
    }

    // Open the bam index file for reading and set the read section.
    samIn.ReadBamIndex(indexFile);
    
    samIn.SetReadSection(chrom.c_str(), startPos0Based, endPos0Based);

    GenomeSequence* refPtr = NULL;
    if(refFile.Length() != 0)
    {
        refPtr = new GenomeSequence(refFile.c_str());
        if(refPtr != NULL)
        {
            PileupElementIndelDiscordance::setReference(*refPtr);
        }
    }

    PileupElementIndelDiscordance::setMinDepth(minDepth);
    PileupElementIndelDiscordance::setPrintPos(printPos);


    // Read the sam records.
    SamRecord record;
    
    // Keep reading records from the file until SamFile::ReadRecord
    // indicates to stop (returns false).
    while(samIn.ReadRecord(samHeader, record))
    {
        // Pileup the bases for this read.
        // This expects 0-based positions.
        pileup.processAlignmentRegion(record, startPos0Based, endPos0Based);
    }

    // Flush the rest of the pileup.
    pileup.flushPileup();

    SamStatus::Status status = samIn.GetStatus();
    if(status == SamStatus::NO_MORE_RECS)
    {
        // A status of NO_MORE_RECS means that all reads were successful.
        status = SamStatus::SUCCESS;
    }

    
    // Determine the average depth.
    double averageDepth =  PileupElementIndelDiscordance::ourRunningDepthStat.Mean();
    // Don't use depths over the average times the multiplier.
    double maxDepth = averageDepth * avgDepthMultiplier;
    
    std::map<uint32_t,
        IndelDiscordance::PileupElementIndelDiscordance::RepeatInfo>::iterator repeatIter;

    if(printCounts)
    {
        std::cerr << "# SUMMARY\n";
        std::cerr << "# numDepth >= " << minDepth << ": " 
                  << PileupElementIndelDiscordance::ourTotalMinDepth << std::endl;
        std::cerr << "# num discordant CIGAR, Depth >= " << minDepth << ": " 
                  << PileupElementIndelDiscordance::ourTotalDiscordant << std::endl;
        for(repeatIter = PileupElementIndelDiscordance::ourRepeatInfo.begin();
            repeatIter != PileupElementIndelDiscordance::ourRepeatInfo.end(); repeatIter++)
        {
            if((*repeatIter).second.discordantCount != 0)
            {
                std::cerr << "# num discordant CIGAR, Depth >= " << minDepth << " with repeats = " 
                          << (*repeatIter).first << ": "
                          << (*repeatIter).second.discordantCount << std::endl;
            }
        }
        for(repeatIter = PileupElementIndelDiscordance::ourRepeatInfo.begin(); 
            repeatIter != PileupElementIndelDiscordance::ourRepeatInfo.end(); repeatIter++)
        {
            std::cerr << "# num Depth >= " << minDepth << " with repeats = " 
                      << (*repeatIter).first << ": "
                      << (*repeatIter).second.count << std::endl;
        }
        std::cerr << "# max depth = " << maxDepth 
                  << std::endl << std::endl << std::endl;
    }

    std::cerr << "#AverageDepth\n" << averageDepth 
              << std::endl << std::endl << std::endl;

    // Calculate the error rate for each repeat.
    std::map<uint32_t, 
        IndelDiscordance::PileupElementIndelDiscordance::DepthInfo>::iterator depthIter;


    std::cerr << "#RepeatCount\tAverageErrorRate\tAverageDepth\tAverageDeletionErrorRate\tAverageInsertionErrorRate\tAvgDeletionLen\tAvgDiscordantDeletionLen\tAvgInsertLen\tAvgDiscordantInsertLen";
    if(sample != "")
    {
        std::cerr << "\tSample";
    }
    if(gender != "")
    {
        std::cerr << "\tGender";
    }
    std::cerr << "\n";

    for(repeatIter = PileupElementIndelDiscordance::ourRepeatInfo.begin(); 
        repeatIter != PileupElementIndelDiscordance::ourRepeatInfo.end(); repeatIter++)
    {
        // Loop through and calculate the error rate for each depth with this
        // repeat.
//         std::cerr << "For RepeatCount = " << (*repeatIter).first << std::endl;

        double sumErrorRates = 0;
        uint32_t numErrorRates = 0;
        double sumDelErrorRates = 0;
        double sumInsErrorRates = 0;
        // Calculate the error rate for each depth that has discordance.
        for(depthIter = (*repeatIter).second.depthInfo.begin(); 
            depthIter != (*repeatIter).second.depthInfo.end(); depthIter++)
        {
            int depth = (*depthIter).first;
            if(depth > maxDepth)
            {
                // this is beyond the max depth we want to include.
                continue;
            }
            // Number of sites with this repeat count and depth
            int count = (*depthIter).second.count;
            // Number of discordant sites with this repeat count and depth
            int numDiscordant = (*depthIter).second.discordantCount;

            if(count != 0)
            {
                // Overall
                double p_discordant = numDiscordant/(double)count;
                double errorRate = 1. - pow(1. - p_discordant, 1./depth);
                sumErrorRates += errorRate * count * (depth-1);
                numErrorRates += count * (depth-1);
                // Deletion
                p_discordant =
                    ((*depthIter).second.delDiscordantCount) / (double)count;
                errorRate =  1. - pow(1. - p_discordant, 1./depth);
                sumDelErrorRates += errorRate * count * (depth-1);
                // Insertion
                p_discordant =
                    ((*depthIter).second.insDiscordantCount) / (double)count;
                errorRate =  1. - pow(1. - p_discordant, 1./depth);
                sumInsErrorRates += errorRate * count * (depth-1);
            }
        }
        if(numErrorRates > 0)
        {
            std:: cerr << (*repeatIter).first << "\t" << sumErrorRates/numErrorRates
                       << "\t" << (*repeatIter).second.runningDepth.Mean()
                       << "\t" << sumDelErrorRates/numErrorRates
                       << "\t" << sumInsErrorRates/numErrorRates
                       << "\t" << (*repeatIter).second.avgDelLens.Mean()
                       << "\t" << (*repeatIter).second.avgDisDelLens.Mean()
                       << "\t" << (*repeatIter).second.avgInsLens.Mean()
                       << "\t" << (*repeatIter).second.avgDisInsLens.Mean();
            if(sample != "")
            {
                std::cerr << "\t" << sample;
            }
            if(gender != "")
            {
                std::cerr << "\t" << gender;
            }
            std::cerr << std::endl;
        }
    }
   



    delete refPtr;
    return(status);
}


IndelDiscordance::PileupElementIndelDiscordance::PileupElementIndelDiscordance()
    : PileupElement()
{
    initVars();
}


IndelDiscordance::PileupElementIndelDiscordance::~PileupElementIndelDiscordance()
{
}


// Add an entry to this pileup element.  
void IndelDiscordance::PileupElementIndelDiscordance::addEntry(SamRecord& record)
{
    // Call the base class:
    PileupElement::addEntry(record);

    ++myDepth;

    // Analyze the record.
    Cigar* cigar = record.getCigarInfo();
    if(cigar == NULL)
    {
        std::cerr << "Failed to retrieve cigar.\n";
        return;
    }
    if(cigar->size() == 0)
    {
        // Cigar not set, so continue.
        return;
    }

    // Get the position in the expanded cigar.
    int cigarPos = 
        cigar->getExpandedCigarIndexFromRefPos(getRefPosition(),
                                               record.get0BasedPosition());
    int newCigarPos = 0;

    char cigarChar = cigar->getCigarCharOp(cigarPos);

    // Check for deletion.
    if(cigarChar == 'D')
    {
        // Deletion
        ++myNumDeletion;
        
        int delLen = 1;

        // Determine the length of the deletion.
        newCigarPos = cigarPos;
        // Search to the left.
        while(newCigarPos > 0)
        {
            --newCigarPos;
            cigarChar = cigar->getCigarCharOp(newCigarPos);
            if(cigarChar == 'D')
            {
                ++delLen;
                // Keep looping looking for more deletions.
            }
            else
            {
                break;
            }
        }
        // Search to the right.
        newCigarPos = cigarPos;
        while(1)
        {
            // Check the next cigarPos.
            ++newCigarPos;
            cigarChar = cigar->getCigarCharOp(newCigarPos);
            if(cigarChar == 'D')
            {
                // deletion
                ++delLen;
                // Keep looping to determine the deletion length.
                continue;
            }
            // Not a deletion, so exit the loop.
            break;
        }
        myDeletionLen.Push(delLen);
    }
    else if((cigarChar == 'M') || (cigarChar == 'X') || (cigarChar == '='))
    {
        ++myNumMatch;
    }
    else
    {
        // Could be a skip or the situation where no matches/deletions and 
        // only insertions.  
        // If only soft clips and insertions, the insertion will not be found
        // if the soft clip is first.
    }

    int insertLen = 0;
    // Check if this is the first position and there is a preceding insert.
    if(getRefPosition() == record.get0BasedPosition())
    {
        newCigarPos = cigarPos;
        while(newCigarPos > 0)
        {
            --newCigarPos;
            cigarChar = cigar->getCigarCharOp(newCigarPos);
            if(cigarChar == 'I')
            {
                ++insertLen;
                // Keep looping looking for more insertions.
            }
            if((cigarChar == 'S') || (cigarChar == 'H'))
            {
                // Clip, so stop looping.
                break;
            }
        }
    }

    // Check to see if the following cigar operation is an insertion.
    // If the next operation is a pad, keep checking.
    newCigarPos = cigarPos;
    while(1)
    {
        // Check the next cigarPos.
        ++newCigarPos;
        cigarChar = cigar->getCigarCharOp(newCigarPos);
        if(cigarChar == 'I')
        {
            // insertion.
            ++insertLen;
            // Keep looping to determine the insert length.
            continue;
        }
        else if(cigarChar == 'P')
        {
            // Only keep checking for insertions if this is a pad.
            continue;
        }
        // Not a pad, so exit the loop.
        break;
    }

    // increment the insertion counters.
    if(insertLen != 0)
    {
        ++myNumInsertion;
        // Check to see if the length equals the average.
        myInsertLen.Push(insertLen);
    }
    else
    {
        ++myNumNoInsertion;
    }
}


// Perform the alalysis associated with this class.  May be a simple print, 
// a calculation, or something else.  Typically performed when this element
// has been fully populated by all records that cover the reference position.
void IndelDiscordance::PileupElementIndelDiscordance::analyze()
{
    // Check the reference for this position.
    char refChar = 'N';
    int numRepeats = 0;
    if(ourReference != NULL)
    {
        // Add one to ref position since genome position takes 1-based
        uint32_t refPos = 
            ourReference->getGenomePosition(getChromosome(),
                                            getRefPosition()+1);
        uint32_t tempRefPos = refPos - 1;
        refChar = (*ourReference)[refPos];
        if(BaseUtilities::isAmbiguous(refChar))
        {
            // Ambiguous base, so skip doing anything.
            return;
        }

        // Have a reference with a base at this position, so check for repeats.
        while(tempRefPos > 0)
        {
            if((*ourReference)[tempRefPos] == refChar)
            {
                ++numRepeats;
            }
            else
            {
                // Not a repeat.
                break;
            }
            --tempRefPos;
        }
        // Loop in the other direction.
        tempRefPos = refPos + 1;
        while(tempRefPos < ourReference->getNumberBases())
        {
            if((*ourReference)[tempRefPos] == refChar)
            {
                ++numRepeats;
            }
            else
            {
                // Not a repeat.
                break;
            }
            ++tempRefPos;
        }
    }

    // Add this position's depth to our average depth value
    // regardless of whether or not it is above the min depth.
    ourRunningDepthStat.Push(myDepth);

    // Increment the running depth stat for this repeat count.
    ourRepeatInfo[numRepeats].runningDepth.Push(myDepth);

    // Update the insertion/deletion lengths.
    if(myNumInsertion != 0)
    {
        ourRepeatInfo[numRepeats].avgInsLens.Push(myInsertLen.Mean());
    }
    if(myNumDeletion != 0)
    {
        ourRepeatInfo[numRepeats].avgDelLens.Push(myDeletionLen.Mean());
    }

    // Only check for discordance if it is above the min depth.
    if(myDepth >= ourMinDepth)
    {
        ++ourTotalMinDepth;

        // Increment the number of sites with this repeat count.
        ++ourRepeatInfo[numRepeats].count;
        // Increment the number of sites with this repeat count and depth.
        ++ourRepeatInfo[numRepeats].depthInfo[myDepth].count;

        bool delDiscordant = false;
        if((myNumDeletion != 0) && (myNumMatch != 0))
        {
            delDiscordant = true;

            ////////////////////////////////////////////
            // Increment the deletion specific counts.
            ++ourRepeatInfo[numRepeats].delDiscordantCount;
            // Increment number of discordant sites with this repeat count
            // and depth.
            ++ourRepeatInfo[numRepeats].depthInfo[myDepth].delDiscordantCount;
            // Update the discordant deletion length.
            ourRepeatInfo[numRepeats].avgDisDelLens.Push(myDeletionLen.Mean());
            // Discordant, so update counts.
            ++ourTotalDelDiscordant;

            ////////////////////////////////////////////
            // Increment the overall discordant counts.
            // Increment number of discordant sites with this repeat count.
            ++ourRepeatInfo[numRepeats].discordantCount;
            // Increment number of discordant sites with this repeat count
            // and depth.
            ++ourRepeatInfo[numRepeats].depthInfo[myDepth].discordantCount;
            // Discordant, so update counts.
            ++ourTotalDiscordant;

            if(ourPrintPos)
            {
                std::cerr << "Position " << getRefPosition() 
                          << " has " << myNumDeletion << " deletions and " 
                          << myNumMatch << " matches.  ";
                if(numRepeats > 0)
                {
                    std:: cerr << numRepeats << " repeats.";
                }
                std::cerr << "\n";
            }
        }
        if((myNumInsertion != 0) && (myNumNoInsertion != 0))
        {
            // Discordance due to insertion.
            ////////////////////////////////////////////
            // Increment the insertion specific counts.
            ++ourRepeatInfo[numRepeats].insDiscordantCount;
            // Increment number of discordant sites with this repeat count
            // and depth.
            ++ourRepeatInfo[numRepeats].depthInfo[myDepth].insDiscordantCount;
            // Update the discordant insertion length.
            ourRepeatInfo[numRepeats].avgDisInsLens.Push(myInsertLen.Mean());

            // Discordant, so update counts.
            ++ourTotalInsDiscordant;

            ////////////////////////////////////////////
            // Increment the overall discordant counts
            // if they have not already been incremented.
            if(!delDiscordant)
            {
                // Increment number of discordant sites with this repeat count.
                ++ourRepeatInfo[numRepeats].discordantCount;
                // Increment number of discordant sites with this repeat count
                // and depth.
                ++ourRepeatInfo[numRepeats].depthInfo[myDepth].discordantCount;
                // Discordant, so update counts.
                ++ourTotalDiscordant;
            }

            if(ourPrintPos)
            {
                std::cerr << "Position " << getRefPosition() 
                          << " has " << myNumInsertion << " insertions and " 
                          << myNumNoInsertion << " no insertions.  ";
                if(numRepeats > 0)
                {
                    std:: cerr << numRepeats << " repeats.";
                }
                std::cerr << "\n";
            }
        }
    }
}


// Resets the entry, setting the new position associated with this element.
void IndelDiscordance::PileupElementIndelDiscordance::reset(int32_t refPosition)
{
    // Call the base class.
    PileupElement::reset(refPosition);

    initVars();
}


void IndelDiscordance::PileupElementIndelDiscordance::initVars()
{
    myDepth = 0;
    myNumDeletion = 0;
    myNumMatch = 0;
    myNumInsertion = 0;
    myNumNoInsertion = 0;
}
