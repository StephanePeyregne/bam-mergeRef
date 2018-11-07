# bam-mergeRef - a command-line tool to merge two versions of a BAM file aligned to different references

This tool was developed as part of a pipeline to avoid reference bias in ancient DNA and was applied to sequences from two low-coverage Neandertal genomes. At positions that differ between the reference and sequenced genome, sequences that carry the non-reference alleles are less likely to align to the reference genome. This is particularly an issue with ancient DNA because of the potential presence of additional substitutions that derive from ancient DNA damage.

If one knows *a priori* polymorphic sites that are likely to differ between the reference and sequenced genomes, one way to avoid this reference bias is to align to two different references carrying one or the other allele, and later combine all alignments.

bam-mergeRef allows one to merge BAM files after they were aligned to different references. The merging proceeds as follows:
- if a sequence aligns to both references at the same position , only one alignment is retained, chosen randomly;
- if a sequence is mapped in one BAM file but unmapped in the other, the mapped sequence is retained;
- if the same sequence is mapped at two different locations or has a different CIGAR string, both sequences are discarded (or collected in a second output BAM file if the user chooses this option)
- unmapped sequences are discarded.

This strategy allows to compensate for the reference bias at known polymorphic positions and has the additional advantage that further sequences are recovered.

## Installation

We provide a make file to compile the code. You will need C++11 and the bamtools library: https://github.com/pezmaster31/bamtools/wiki/Building-and-installing

## Input

The two BAM files must be sorted by name before using ban-mergeRef. You can use samtools sort (http://www.htslib.org/doc/samtools.html) to sort your BAM files with the option -n.

## Example of command line
```
bam-mergeRef -a <reference name 1> -b <reference name 2> <input BAM file 1> <input BAM file 2> <output BAM file> -t [trashfile]
```
where reference names are IDs that will be saved in the header of the output BAM file. The option -t allows you to specify the name of a BAM file that will contain all discarded alignments. 

## Other relevant information:
- Note that you should probably generate the MD field again on the output file, to have MD fields based on one reference only. 

 - bam-mergeRef adds a new field (RN, for reference number) in the alignments of the output file, which can be RN:i:1 (alignment to reference 1), RN:i:2 (alignment to reference 2) or RN:i:12 (alignment to both reference 1 and 2) depending on the origin of the alignment.

 - The information about the references is encoded in the header, in the @SQ lines. bam-mergeRef adds an alternative name flag (AN) with the name of the chromosome (or sequence), the reference ID provided to bam-mergeRef and the file number (which matches the number in the RN flags in the alignment section). For instance, AN:MT-hg19-1 means that alignments to the mitochondrial sequence from the hg19 human reference come from file number 1.

- If you removed unmapped reads before merging and two mates are present in one BAM file but only one mate is present in the other, the three sequences are discarded.
