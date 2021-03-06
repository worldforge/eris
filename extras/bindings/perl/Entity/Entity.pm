package WorldForge::Eris::Entity;

use 5.006;
use strict;
use warnings;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use WorldForge::Eris::Entity ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);
our $VERSION = '0.01';

bootstrap WorldForge::Eris::Entity $VERSION;

# Preloaded methods go here.

use WorldForge::Eris::World;

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

WorldForge::Eris::Entity - Perl extension for blah blah blah

=head1 SYNOPSIS

  use WorldForge::Eris::Entity;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for WorldForge::Eris::Entity, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.


=head1 AUTHOR

A. U. Thor, E<lt>a.u.thor@a.galaxy.far.far.awayE<gt>

=head1 SEE ALSO

L<perl>.

=cut
