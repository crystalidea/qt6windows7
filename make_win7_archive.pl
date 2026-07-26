#!/usr/bin/env perl
#
# Packs the DLLs of a Qt installation built with this backport into the archive
# published as a release asset (qt6_x64_to_run_on_windows7.7z and its x86 twin).
#
# Usage:
#   perl make_win7_archive.pl <path to Qt installation> [output archive]
#
#   perl make_win7_archive.pl C:\qt6_x64      -> qt6_x64_to_run_on_windows7.7z
#   perl make_win7_archive.pl C:\qt6          -> qt6_x86_to_run_on_windows7.7z
#
# The architecture is read from Qt6Core.dll rather than assumed, since it decides
# both the default archive name and which Visual C++ runtime to pack - pairing a
# 32-bit Qt with the 64-bit runtime would produce an archive that cannot run.
#
# In go the Qt libraries from bin\, the plugins listed below, Qt Designer - there
# to try the build out right away - and the Visual C++ runtime it all links
# against. Debug builds (the ...d.dll twins) are skipped.
#
# The runtime is not ours, but leaving it out does not work: a redistributable
# older than the toolset that compiled Qt is unsupported and crashes rather than
# refusing to load. A Windows 7 machine carrying, say, 14.36 will start
# designer.exe built with 14.44 and then fault inside MSVCP140.dll. Shipping the
# matching one keeps the archive self-contained.

use strict;
use warnings;
use File::Basename;
use File::Copy;
use File::Path qw(remove_tree make_path);
use File::Spec;

# Plugin folders to include, each copied to a folder of the same name in the
# archive. Loaded by path convention at run time, so the names matter.
# Add more here if an application needs them (sqldrivers, iconengines, ...).
my @plugin_dirs = qw(platforms styles imageformats multimedia);

# Programs to ship alongside the libraries, from bin\. Qt Designer exercises a
# good part of the stack - widgets, styles, image formats, printing - so it is a
# quick way to tell whether a build actually runs on Windows 7. Skipped silently
# when the module providing it was not built.
my @programs = qw(designer.exe);

# Set to 0 to leave the Visual C++ runtime out and require the redistributable to
# be installed on the target machine - see the note at the top before doing so.
my $include_msvc_runtime = 1;

my $qt_dir  = shift @ARGV;
my $archive = shift @ARGV;

usage("no Qt installation given") unless defined $qt_dir;
usage("'$qt_dir' is not a directory") unless -d $qt_dir;
usage("'$qt_dir' has no bin directory - is it a Qt installation?") unless -d "$qt_dir\\bin";

my $architecture = qt_architecture($qt_dir);

usage("could not tell whether '$qt_dir' is a 32- or 64-bit build") unless $architecture;

$archive = "qt6_${architecture}_to_run_on_windows7.7z" unless defined $archive;

printf "%-14s %s\n", "architecture", $architecture;

my $seven_zip = find_7zip();

# Stage everything first, so the archive ends up with clean relative paths
# regardless of where the Qt installation lives.
my $staging = "_win7_archive_staging";

remove_tree($staging) if -d $staging;
make_path($staging) or die "Could not create '$staging': $!\n";

my $total = 0;

# 1. Qt libraries, into the root of the archive
my @libs = grep { !is_debug_build($_) } glob("$qt_dir\\bin\\Qt6*.dll");

die "No Qt6*.dll found in '$qt_dir\\bin'\n" unless @libs;

for my $dll (@libs) {
    copy($dll, $staging) or die "Failed to copy $dll: $!\n";
    $total++;
}

printf "%-14s %d libraries\n", "bin", scalar @libs;

# 2. Programs, next to the libraries in the root of the archive
for my $program (@programs) {
    my $src = "$qt_dir\\bin\\$program";

    if (!-e $src) {
        printf "%-14s skipped (not built)\n", $program;
        next;
    }

    copy($src, $staging) or die "Failed to copy $src: $!\n";
    $total++;

    printf "%-14s included\n", $program;
}

# 3. Plugins, each into a folder of its own
for my $dir (@plugin_dirs) {
    my $src = "$qt_dir\\plugins\\$dir";

    if (!-d $src) {
        printf "%-14s skipped (not built)\n", $dir;
        next;
    }

    my @plugins = grep { !is_debug_build($_) } glob("$src\\*.dll");

    if (!@plugins) {
        printf "%-14s skipped (empty)\n", $dir;
        next;
    }

    make_path("$staging\\$dir") or die "Could not create '$staging\\$dir': $!\n";

    for my $dll (@plugins) {
        copy($dll, "$staging\\$dir") or die "Failed to copy $dll: $!\n";
        $total++;
    }

    printf "%-14s %d plugins (%s)\n", $dir, scalar @plugins,
           join(", ", map { basename($_) } @plugins);
}

# 4. Visual C++ runtime, next to the programs that link it
if ($include_msvc_runtime) {
    my $crt = find_vc_redist($architecture);

    if (!$crt) {
        print STDERR "\nWARNING: no $architecture Visual C++ redistributable found - the archive will\n";
        print STDERR "only run where one at least as new as the compiler is already installed.\n\n";
    }
    else {
        my @runtime = glob("\"$crt\\*.dll\"");

        for my $dll (@runtime) {
            copy($dll, $staging) or die "Failed to copy $dll: $!\n";
            $total++;
        }

        printf "%-14s %d libraries (%s, %s)\n", "msvc runtime", scalar @runtime,
               $architecture, basename($crt);
    }
}

# 5. Archive the staged tree. The old archive is removed first: 7-Zip adds to an
#    existing one, which would keep files from a previous run.
unlink($archive) if -e $archive;

my $archive_path = File::Spec->rel2abs($archive);

chdir($staging) or die "Cannot chdir to '$staging': $!\n";

system($seven_zip, "a", "-t7z", "-mx=9", $archive_path, "*") == 0
  or die "7-Zip failed: $?\n";

chdir("..") or die "Cannot chdir back: $!\n";

remove_tree($staging);

printf "\n%d files -> %s (%.1f MB)\n", $total, $archive, (-s $archive_path) / 1024 / 1024;

# A debug build is the "...d.dll" twin of a release library. Checking for that
# twin matters: qdirect2d.dll, qopensslbackend.dll and a few others genuinely
# end in a 'd' and would be dropped by a plain name match.
sub is_debug_build
{
    my ($path) = @_;

    return 0 unless $path =~ /d\.dll$/i;

    (my $release = $path) =~ s/d\.dll$/.dll/i;

    return -e $release ? 1 : 0;
}

# Whether the Qt installation is a 32- or 64-bit build, read from the machine
# field of Qt6Core.dll's PE header: 4 bytes at 0x3C point at the PE signature,
# and the machine type follows it.
sub qt_architecture
{
    my ($dir) = @_;

    my $core = "$dir\\bin\\Qt6Core.dll";

    open(my $dll, '<:raw', $core) or return undef;

    my $buffer;

    seek($dll, 0x3C, 0) and read($dll, $buffer, 4) == 4 or return undef;

    my $pe_offset = unpack('V', $buffer);

    seek($dll, $pe_offset, 0) and read($dll, $buffer, 6) == 6 or return undef;

    my ($signature, $machine) = unpack('a4v', $buffer);

    close($dll);

    return undef unless $signature eq "PE\0\0";

    return 'x64' if $machine == 0x8664;
    return 'x86' if $machine == 0x014C;

    return undef;
}

# The CRT folder of the Visual C++ redistributable, for the architecture Qt was
# built for. Inside a Developer Command Prompt the location is already in the
# environment; otherwise look through the Visual Studio installations and take
# the newest, which is what the most recently installed toolset compiles against.
sub find_vc_redist
{
    my ($arch) = @_;

    my @candidates;

    if (defined $ENV{'VCToolsRedistDir'}) {
        my $dir = $ENV{'VCToolsRedistDir'};

        $dir =~ s/\\+$//;

        push @candidates, glob("\"$dir\\$arch\\Microsoft.VC*.CRT\"");
    }

    for my $program_files ($ENV{'ProgramFiles'}, $ENV{'ProgramFiles(x86)'}) {
        next unless defined $program_files;

        push @candidates,
          glob("\"$program_files\\Microsoft Visual Studio\\*\\*\\VC\\Redist\\MSVC\\*\\$arch\\Microsoft.VC*.CRT\"");
    }

    my @found = grep { -e "$_\\msvcp140.dll" } @candidates;

    return undef unless @found;

    @found = sort { redist_version($b) <=> redist_version($a) } @found;

    return $found[0];
}

# Sortable number for the version in a redistributable path, so that 14.44 wins
# over 14.9 - which plain string comparison would get backwards.
sub redist_version
{
    my ($path) = @_;

    return 0 unless $path =~ /MSVC\\(\d+)\.(\d+)\.(\d+)/;

    return $1 * 1_000_000 + $2 * 1_000 + $3;
}

sub find_7zip
{
    for my $dir ($ENV{'ProgramFiles'}, $ENV{'ProgramFiles(x86)'}) {
        next unless defined $dir;

        my $exe = "$dir\\7-Zip\\7z.exe";

        return $exe if -e $exe;
    }

    # Fall back to whatever is on PATH.
    return "7z";
}

sub usage
{
    my ($message) = @_;

    print STDERR "$message\n\n" if $message;
    print STDERR "Usage: perl $0 <path to Qt installation> [output archive]\n";
    print STDERR "   eg: perl $0 C:\\qt6_x64\n";

    exit 1;
}
