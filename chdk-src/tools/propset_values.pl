# Simple perl script to output all defined PROPSET values as a table
# run perl tools/propset_values.pl > doc/propset_values.csv to update file

use IO::Handle;

my %pnam;

sub load
{
    my ($f) = @_;
    my $g = "grep \"^#define PROPCASE_\" include/propset$f.h | awk '{printf \"%s %s\\n\", \$2, \$3;}' | sort > p.tmp";
    `$g`;

    open IN,"<p.tmp" or die;

    my %pv;
    
    while (<IN>)
    {
        chomp;
        ($h,$n) = $_ =~ /^(.*) (.*)$/;
        $pnam{$h} = 1;
        $pv{$h} = $n;
    }

    close IN;

    `rm p.tmp`;

    return %pv;
}

my %p1 = load("1");
my %p2 = load("2");
my %p3 = load("3");
my %p4 = load("4");
my %p5 = load("5");
my %p6 = load("6");
my %p7 = load("7");
my %p8 = load("8");
my %p9 = load("9");
my %p10 = load("10");
my %p11 = load("11");
my %p12 = load("12");
my %p13 = load("13");

# Note: columns ordered by increasing values on the majority of properties (excluding 1)
print "NAME,P1,P2,P3,P6,P4,P5,P8,P7,P9,P10,P13,P11,P12\n";

foreach my $k (sort keys %pnam)
{
    print "$k,$p1{$k},$p2{$k},$p3{$k},$p6{$k},$p4{$k},$p5{$k},$p8{$k},$p7{$k},$p9{$k},$p10{$k},$p13{$k},$p11{$k},$p12{$k}\n";
}

print "\nGenerated file - run 'perl tools/propset_values.pl > doc/propset_values.csv' to update\n"
