# internalDevelop

This would be our internal repo for development. It adds **common**, **blas** and **solver** to build and test togetehr rocBLAS and rocSOLVER libraries. CI will test integration of changes on the three components (so that changes on the common and/or blas sides don't break solver, for example). Changes are then *backported* to their corresponding internal repos so that they are available to other projects. 
