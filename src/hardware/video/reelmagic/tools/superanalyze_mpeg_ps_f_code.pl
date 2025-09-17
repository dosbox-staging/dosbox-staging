#!/usr/bin/perl

## Copyright (C) 2022 Jon Dennis
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.



use strict;
use FindBin;
use IPC::Open2;


my $filename = shift;
die unless $filename;

my $target_f_code = shift;
$target_f_code += 0;

my $sa_pid = open2(my $sa_stdout, my $sa_stdin,
  $FindBin::Bin . '/superanalyze_mpeg_ps', $filename);
die unless $sa_pid;
close($sa_stdin);

my $current_picture_is_target = 0;
my $gop_index = 0;
my $picture_type = 'I';
my $picture_index = 0;
my $picture_gop_index = 0;
my $forward_f_code = 1;
my $backward_f_code = 1;
my $backward_f_code = 1;
my $temporal_seqnum = 0;

while (<$sa_stdout>) {
  if (/^\s*GOP #(\d+)/) {
    $gop_index = $1 + 0;
    $picture_gop_index = -1;
    next;
  }
  if (/^\s*(\w) Picture #(\d+)/) {
    ($picture_type, $picture_index) = ($1, $2 + 0);
    ++$picture_gop_index;
    $current_picture_is_target = is_target_picture();
    next;
  }
  next unless $current_picture_is_target;
  if (/^\s*-\s*Forward F Code: (\d+)/) {
    $forward_f_code = $1 + 0;
    next;
  }
  if (/^\s*-\s*Backward F Code: (\d+)/) {
    $backward_f_code = $1 + 0;
    next;
  }
  if (/^\s*-\s*Temporal Sequence Number: (\d+)/) {
    $temporal_seqnum = $1 + 0;
    next;
  }
  if (/^\s*-\s*VBV Delay/) { #VBV delay is last
      my $bfcode = ($picture_type eq 'B') ? "bfcode=$backward_f_code" : '';
      printf "tsn=%02u ffcode=$forward_f_code $bfcode", $temporal_seqnum;
      print_delta();
      print "\n";
    next;
  }
}


wait;


sub is_target_picture {
  return 0 if $picture_type eq 'I';
  return 1;
}

sub print_delta {
  return unless $target_f_code;
  
  if (abs($target_f_code - $forward_f_code) < abs($forward_f_code - $target_f_code)) {
    print ' delta=' . ($forward_f_code - $target_f_code);
  }
  else {
    print ' delta=' . ($target_f_code - $forward_f_code);
  }
}

## my @sum_codes = (
##   -1,  # tsn=0
##   -2,  # tsn=1
##   +1,  # tsn=2
##    0,  # tsn=3
##   +2,  # tsn=4
##   +1,  # tsn=5
##   -3,  # tsn=6
##   +3,  # tsn=7
##    0,  # tsn=8
##   -1,  # tsn=9
##   +2,  # tsn=10
##   +1,  # tsn=11
##   +3,  # tsn=12
##   +2,  # tsn=13
##   -2,  # tsn=14
## );

