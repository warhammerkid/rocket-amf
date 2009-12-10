require 'rocketamf/pure/serializer'
require 'rocketamf/pure/remoting'
require 'ramf_ext'

module RocketAMF
  # This module is a wrapper around the C extensions
  module Ext
    $DEBUG and warn "Using C library for RocketAMF."
  end

  #:stopdoc:
  # Import deserializer
  Deserializer = AMF::C::AMF0Deserializer
  AMF3Deserializer = AMF::C::AMF3Deserializer

  # Import serializer
  Serializer = RocketAMF::Pure::Serializer
  AMF3Serializer = RocketAMF::Pure::AMF3Serializer

  # Modify request and response so they can serialize/deserialize
  class Request
    remove_method :populate_from_stream
    include RocketAMF::Pure::Request
  end

  class Response
    remove_method :serialize
    include RocketAMF::Pure::Response
  end
  #:startdoc:
end