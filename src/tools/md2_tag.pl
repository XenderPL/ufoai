#!/usr/bin/perl
#######################################
# Copyright (C) 2006 Werner H�rer
# Changed to read tag files M. Gerhardy
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#See the GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#######################################

#######################################
# Usage
#	md2_tag.pl [tagfile]
#######################################
use strict;
use warnings;

# Default values for filepaths
package MD2_tag;
use base 'Parse::Binary';

use constant FORMAT => (
	Indent		=> 'I',			# should be 844121162
	Version		=> 'I',			# tag version. must be equal to 1

	NumTags		=> 'I',			# number of textures
	NumFrames		=> 'I',		# number of vertices (x,y,z)

	OffsetNames		=> 'I',		# offset to tag names (64 bytes each)
	OffsetTags		=> 'I',		# offset
	OffsetEnd		=> 'I',		# offset
	OffsetExtractEnd	=> 'I',	# offset

	TagName			=> ['a64', '{$NumTags}', 1 ],	# 64chars * NumTags
	Data			=> ['f12', '{$NumTags*$NumFrames}', 12 ]	# 12 floats * NumTags * NumFrames
);

package TagName;
use base 'Parse::Binary';
use constant FORMAT => ('a64');

package Data;
use base 'Parse::Binary';
use constant FORMAT => ('f12');

#package Data;
#use base 'Parse::Binary';
#use constant FORMAT => (
#	Row1_x => 'f',
#	Row1_y => 'f',
#	Row1_z => 'f',
#	Row2_x => 'f',
#	Row2_y => 'f',
#	Row2_z => 'f',
#	Row3_x => 'f',
#	Row3_y => 'f',
#	Row3_z => 'f',
#	Row4_x => 'f',
#	Row4_y => 'f',
#	Row4_z => 'f'
#);


#######################################
# MAIN
#######################################
use Data::Dumper;
my $tagfile = "";
my $verbose = 0;

# parse commandline paarameters (tag-filenames)
if ( $#ARGV < 0 ) {
	die "Usage:\tmd2_tag.pl [tagfile] [verbose]\n";
} elsif ( $#ARGV == 0 ) {
	$tagfile = $ARGV[0];
} elsif ( $#ARGV == 1 ) {
	$tagfile = $ARGV[0];
	$verbose = 1;
}

# read .md2 file
my $md2tag_file = MD2_tag->new($tagfile);

die "File has wrong magic number \"".$md2tag_file->Indent."\".\n" unless ($md2tag_file->Indent == 844121162);
die "File has wrong format version \"".$md2tag_file->Version."\".\n" unless ($md2tag_file->Version == 1);


print $md2tag_file->NumTags, " Tags found\n";
print $md2tag_file->NumFrames, " Frames found\n";
print "Tagnames:\n";

for (my $i = 0; $i < $md2tag_file->NumTags; $i++) {
	my $name = $md2tag_file->TagName->[$i][0];
	$name =~ s/\0*//sg; # Remove all NUL from the string.
	chomp($name);
	print "- ",$name,"\n";
}

if ($verbose) {
	print "Coords:\n";
	#print Dumper($md2tag_file);
	for (my $tags = 0; $tags < $md2tag_file->NumTags; $tags++) {
		my $name = $md2tag_file->TagName->[$tags][0];
		$name =~ s/\0*//sg; # Remove all NUL from the string.
		print "# tag: ". $name."\n";
		my $frame = 0;
		for (my $j = $tags * $md2tag_file->NumFrames; $j < ($tags+1) * $md2tag_file->NumFrames; $j++) {
			$frame++;
			print "Frame ",$frame," --- ",$name," ----------------------\n";
			print "Row1 ",$md2tag_file->Data->[$j][0];
			print "  ",$md2tag_file->Data->[$j][1];
			print "  ",$md2tag_file->Data->[$j][2];
			print "\n";
			print "Row2 ",$md2tag_file->Data->[$j][3];
			print "  ",$md2tag_file->Data->[$j][4];
			print "  ",$md2tag_file->Data->[$j][5];
			print "\n";
			print "Row3 ",$md2tag_file->Data->[$j][6];
			print "  ",$md2tag_file->Data->[$j][7];
			print "  ",$md2tag_file->Data->[$j][8];
			print "\n";
			print "Row4 ",$md2tag_file->Data->[$j][9];
			print "  ",$md2tag_file->Data->[$j][10];
			print "  ",$md2tag_file->Data->[$j][11];
			print "\n";
		}
	}
}

__END__

##########
# EOF
##########
