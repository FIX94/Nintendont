#!/usr/bin/perl
# GameTDB MD5 parser.
# Takes a GameTDB XML file and converts it into a flat
# MD5 database for use with Nintendont's MD5 verifier.

# XML Parser references:
# - http://mkweb.bcgsc.ca/intranet/perlbook/pxml/ch03_02.htm
# - http://mkweb.bcgsc.ca/intranet/perlbook/pxml/ch04_06.htm

use strict;
use warnings;
use XML::Parser;
use Switch;

# Use UTF-8.
# Reference: http://stackoverflow.com/questions/627661/how-can-i-output-utf-8-from-perl
use utf8;
binmode(STDOUT, ":utf8");

# Get the XML file to parse.
my $xmlfile = shift @ARGV;
if (!defined($xmlfile)) {
	print STDERR "A WiiTDB XML file must be specified for processing.\n";
	exit 1;
}

# Initialize the parser object and parse the string.
my $parser = XML::Parser->new(
	ErrorContext => 2,
	ProtocolEncoding => 'UTF-8',
	Handlers => {
		Init =>		\&handle_doc_start,
		Final =>	\&handle_doc_end,
		Start =>	\&handle_elem_start,
		End =>		\&handle_elem_end,
		Char =>		\&handle_char_data,
	}
);

my $lang;	# Locale language in the current <locale> element.
my $record;	# points to a hash of element contents
my @context;	# array of current elements (highest index is current)

eval { $parser->parsefile($xmlfile); };

# Check for a parse error.
if ($@) {
	$@ =~ s/at \/.*?$//s;	# Remove module line number.
	print STDERR "\nERROR in '$xmlfile':\n$@\n";
	exit 1;
}

exit 0;

### Handlers. ###

# Start of document.
sub handle_doc_start {
	# No header needed.
}

# Start of element.
sub handle_elem_start {
	my ($expat, $name, %atts) = @_;
	push @context, $name;

	# Check the element name.
	# TODO: Only allow 'game' in top-level elements.
	switch ($name) {
		case 'game' {
			# New game entry.
			$record = {};
			$record->{'name'} = $atts{'name'};
			$record->{'md5s'} = {};
			$lang = '';
		}
		case 'rom' {
			# ROM information.
			my $md5s = $record->{'md5s'};
			my $version = $atts{'version'};
			if (defined($md5s->{$version})) {
				print STDERR "WARNING: Duplicate version $atts{'version'} for $record->{'id6'}.\n";
				do {
					$version = '!'.$version;
				} while (defined($md5s->{$version}));
			}
			$md5s->{$version} = $atts{'md5'};
		}
		case 'locale' {
			# Save the current locale language for later.
			$lang = $atts{'lang'};
		}
	}
}

# Collect character data into the recent element's buffer.
sub handle_char_data {
	my ($expat, $text) = @_;

	switch ($context[scalar(@context)-1]) {
		case 'id' {
			# ID6.
			$record->{'id6'} = $text;
		}
		case 'type' {
			# Game type.
			# We only want retail GameCube games.
			$record->{'type'} = $text;
		}
		case 'title' {
			# Title. (Only if lang='EN'.)
			# TODO: Other titles for other regions?
			if (defined($lang) && $lang eq 'EN') {
				$record->{'title'} .= $text;
			}
		}
	}
}

# Parse a GameTDB version.
# Returns the revision number and disc number.
sub parse_version {
	my ($version, $disc_offset) = @_;
	if (!defined($version) || $version eq '') {
		# Empty version field. Assume Rev.00 and a
		# single-disc game unless $disc_offset is non-zero.
		return ('00', $disc_offset);
	}

	# Revision number.
	my $rev = '00';
	if ($version =~ /1.1$/ ||
	    $version =~ /1.01$/)
	{
		$rev = '01';
	} elsif ($version =~ /1.02$/) {
		$rev = '02';
	} elsif ($version =~ /1.03$/) {
		$rev = '03';
	} elsif ($version =~ /1.05$/) {
		$rev = '05';
	} elsif ($version =~ /1.48$/) {
		$rev = '48';
	} elsif ($version =~ /^disc ?\d$/i) {
		# Disc number only. Assume Rev.00.
		$rev = '00';
	} else {
		# Check for generic 1.0 versions.
		switch (lc($version)) {
			case '1'	{ $rev = '00'; }
			case '1.'	{ $rev = '00'; }
			case '1.0'	{ $rev = '00'; }
			case '1.00'	{ $rev = '00'; }
			else { $rev = "UNHANDLED VERSION $version"; }
		}
	}

	# Disc number.
	my $discnum = 0;
	if ($version =~ /^disc ?(\d)/i) {
		$discnum = $1;
	} elsif ($version =~ /disc/i) {
		# Unhandled disc number.
		print STDERR "WARNING: Unhandled disc number $version.\n";
		$discnum = -1;
	}
 
	return ($rev, $discnum + $disc_offset);
}

# End of element.
sub handle_elem_end {
	my ($expat, $name) = @_;
	pop @context;
	return unless($name eq 'game');

	# Game type must be 'GameCube'.
	return unless(defined($record->{'type'}) && $record->{'type'} eq 'GameCube');

	# Database entry format:
	# MD5|ID6|Rev|Disc#|Title
	my $md5s = $record->{'md5s'};

	# Check for 'disc0', in which case we have to use an offset for multi-disc.
	my $disc_offset = 0;
	if (defined($md5s->{'disc0'}) || defined($md5s->{'Disc0'})) {
		# Disc 0 is present.
		$disc_offset = 1;
	}

	# Parse all revisions.
	my @keys = sort(keys(%$md5s));
	foreach my $version (@keys) {
		if (defined($md5s->{$version}) && length($md5s->{$version}) == 32) {
			my ($rev, $discnum) = parse_version($version, $disc_offset);
			print $md5s->{$version}.'|'.$record->{'id6'}.'|'.$rev.'|'.$discnum.'|'.$record->{'title'}."\n";
		}
	}
}

# End of document.
sub handle_doc_end {
	# No footer needed.
}
