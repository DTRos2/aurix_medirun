import sys
if sys.prefix == '/usr':
    sys.real_prefix = sys.prefix
    sys.prefix = sys.exec_prefix = '/home/cce/main_project/aurix_main_project_01/install/medirun_navigation'
