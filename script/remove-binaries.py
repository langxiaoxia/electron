#!/usr/bin/env python
from __future__ import print_function
import argparse
import os
import sys

from lib.config import LINUX_BINARIES, enable_verbose_mode
from lib.util import execute, get_out_dir

def remove_binaries(directory):
  for binary in LINUX_BINARIES:
    binary_path = os.path.join(directory, binary)
    if os.path.isfile(binary_path):
      remove_binary(binary_path)

def remove_binary(binary_path):
  remove = 'rm'
  execute([
    remove, '-f',
    binary_path])

def main():
  args = parse_args()
  if args.verbose:
    enable_verbose_mode()
  if args.file:
    remove_binary(args.file)
  else:
    remove_binaries(args.directory)

def parse_args():
  parser = argparse.ArgumentParser(description='Remove linux binaries')
  parser.add_argument('-d', '--directory',
                      help='Path to the dir that contains files to remove.',
                      default=get_out_dir(),
                      required=False)
  parser.add_argument('-f', '--file',
                      help='Path to a specific file to remove.',
                      required=False)
  parser.add_argument('-v', '--verbose',
                      action='store_true',
                      help='Prints the output of the subprocesses')

  return parser.parse_args()

if __name__ == '__main__':
  sys.exit(main())
