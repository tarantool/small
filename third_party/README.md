Valgrind
========

Valgrind header files were extracted from valgrind 3.12.0 source tarball
available from http://valgrind.org/:

6eb03c0c10ea917013a7622e483d61bb  valgrind-3.12.0.tar.bz2

valgrind/valgrind.h was patched to fix -Wunused-value warnings with
-DNVALGRIND=1:

index 8f29f28..b425be5 100644
--- a/third_party/valgrind/valgrind.h
+++ b/third_party/valgrind/valgrind.h
@@ -214,7 +214,7 @@
 #define VALGRIND_DO_CLIENT_REQUEST_EXPR(                          \
         _zzq_default, _zzq_request,                               \
         _zzq_arg1, _zzq_arg2, _zzq_arg3, _zzq_arg4, _zzq_arg5)    \
-      (_zzq_default )
+      ({(void) _zzq_default; _zzq_default; })
