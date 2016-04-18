use strict;
use warnings;
use Digest::MD5 qw(md5_hex);
use File::Temp qw(tempdir);
use Net::EmptyPort qw(check_port empty_port);
use Test::More;
use t::Util;

subtest "fastcgi" => sub {
    my $server = spawn_h2o(<< "EOT");
file.custom-handler:
  extension: .cgi
  fastcgi.spawn: "exec \$H2O_ROOT/share/h2o/fastcgi-cgi"
  unsetenv:
    - "foo"
setenv:
  "global": 123
hosts:
  default:
    setenv:
      "host": 234
    paths:
      "/":
        setenv:
          "path": 345
          "foo": "abc"
        file.dir: @{[ DOC_ROOT ]}
EOT
    run_with_curl($server, sub {
        my ($proto, $port, $curl) = @_;
        my $resp = `$curl --silent $proto://127.0.0.1:$port/printenv.cgi`;
        like $resp, qr{^global:123$}im;
        like $resp, qr{^host:234$}im;
        like $resp, qr{^path:345$}im;
        unlike $resp, qr{^foo:}im;
    });
};

done_testing();