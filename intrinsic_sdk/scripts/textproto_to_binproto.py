#!/usr/bin/env python3

import argparse
import sys

from google.protobuf import text_format
from google.protobuf import descriptor_pb2
from google.protobuf.descriptor_pool import DescriptorPool
from google.protobuf.message_factory import MessageFactory


def build_descriptor_pool(databases):
    """
    Given an input filename pointing to a protobuf descriptor database file,
    create a descriptor pool
    """
    descriptor_pool = DescriptorPool()

    for filename in databases:
        fds = descriptor_pb2.FileDescriptorSet()
        with open(filename, 'rb') as f:
            fds.ParseFromString(f.read())

        for descriptor_file in fds.file:
            try:
                descriptor_pool.Add(descriptor_file)
            except:
                # In the case that we have a duplicate, keep going
                pass

    return descriptor_pool


def create_message(descriptor_pool, message_type):
    """
    Create a message instance of message_type from a descriptor_pool
    """
    factory = MessageFactory(descriptor_pool)
    message_descriptor = descriptor_pool.FindMessageTypeByName(message_type)
    message_prototype = factory.GetPrototype(message_descriptor)
    message = message_prototype()
    return message


def main(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument("--descriptor_database", nargs="+", help="Path to the descriptor database")
    parser.add_argument("--message_type", help="Message type of the textproto file")
    parser.add_argument("--textproto_in", nargs="?", help="Textproto format data")
    parser.add_argument("--binproto_out", nargs="?", help="Output destination for binproto format")

    args = parser.parse_args(argv)

    try:
        descriptor_pool = build_descriptor_pool(args.descriptor_database)
    except Exception as ex:
        print(f"Error: Failed to build descriptor pool from database: {ex}", file=sys.stderr)
        return -1

    message = create_message(descriptor_pool, args.message_type)

    if args.textproto_in:
        try:
            with open(args.textproto_in, 'r') as file:
                content = file.read()
        except FileNotFoundError:
            print(f"Error: Failed to input file '{args.textproto_in}' for read.", file=sys.stderr)
            return -1
    else:
        content = sys.stdin.read()

    try:
        text_format.Parse(content, message, descriptor_pool=descriptor_pool)
        message_string = message.SerializeToString()
    except text_format.ParseError as e:
        print(f"Error: failed to parse textproto: {e}", file=sys.stderr)
        return -1

    if args.binproto_out:
        try:
            with open(args.binproto_out, 'wb') as file:
                file.write(message_string)
        except:
            print(f"Error: Failed to open '{args.filename}' for write.", file=sys.stderr)
            return -1
    else:
        sys.stdout.write(str(message_string))

if __name__ == "__main__":
    sys.exit(main())
