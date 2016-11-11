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
my %records;	# set of game entries

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
			$lang = '';
		}
		case 'rom' {
			# ROM information.
			# TODO: Convert the various 'version' formats to two-digit.
			$record->{'version'} = $atts{'version'};
			$record->{'md5'} = $atts{'md5'};
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
				$record->{'title'} = $text;
			}
		}
	}
}

# End of element.
sub handle_elem_end {
	my ($expat, $name) = @_;
	pop @context;
	return unless($name eq 'game');

	# Game type must be 'GameCube'.
	return unless(defined($record->{'type'}) && $record->{'type'} eq 'GameCube');

	# MD5 must exist and be 32 characters long.
	return unless(defined($record->{'md5'}) && length($record->{'md5'}) == 32);

	# Print the game information.
	print $record->{'md5'}.'|'.$record->{'id6'}.'|'.$record->{'version'}.'|'.$record->{'title'}."\n";
}

# End of document.
sub handle_doc_end {
	# No footer needed.
}
