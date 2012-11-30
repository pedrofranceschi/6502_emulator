class AddressingModes
  AddressingModes = 0,
  Implicit = 1, # null when assembling (reference)
  Accumulator = 2,
  Immediate = 3,
  ZeroPage = 4,
  ZeroPageX = 5,
  ZeroPageY = 6,
  Relative = 7, # NEEDS TO IMPLEMENT
  Absolute = 8,
  AbsoluteX = 9,
  AbsoluteY = 10,
  Indirect = 11,
  IndexedIndirect = 12, # ($40,X)
  IndirectIndexed = 13  # ($40),Y
end

class String
  def to_hex
    return self.to_i(16).to_s(10).to_i
  end
end

class Assembler

  def cleanup_byte_string(byte_string)
    return byte_string.tr('$#(),.XY;', '')
  end

  def high_low_part_of_byte(byte)
    return [ (byte & 0xff), (byte >> 0x8) ]  # high byte part, low byte part
  end

  def parse_line(line)
    line_parser = line.split(" ")
    if line_parser.size == 1
      return {
        :instruction => line_parser[0],
        :addressMode => AddressingModes::Implicit
      }
    else
      instruction = line_parser[0]
      line_parser.delete(instruction)
      parameters = line_parser.join

      if parameters == "A"
        return {
          :instruction => instruction,
          :addressMode => AddressingModes::Accumulator
        }
      elsif parameters[0] == "#"
        immediate_byte = cleanup_byte_string(parameters).to_hex
        raise "Error at line '#{line}': only 1 byte is allowed." if immediate_byte > 0xff

        return {
          :instruction => instruction,
          :addressMode => AddressingModes::Immediate,
          :parameters => [ immediate_byte ]
        }
      elsif parameters[0] == "$"
        if parameters.index(",") # zero page,X  or  zero page,Y
          page_parser = parameters.split(",")
          byte = cleanup_byte_string(page_parser[0]).to_hex

          if page_parser[1] == "X"
            return {
              :instruction => instruction,
              :addressMode => (byte > 0xff ? AddressingModes::AbsoluteX : AddressingModes::ZeroPageX),
              :parameters => (byte > 0xff ? high_low_part_of_byte(byte) : [ byte ] )
            }
          elsif page_parser[1] == "Y"
            return {
              :instruction => instruction,
              :addressMode => (byte > 0xff ? AddressingModes::AbsoluteY : AddressingModes::ZeroPageY),
              :parameters => (byte > 0xff ? high_low_part_of_byte(byte) : [ byte ] )
            }
          else
            raise "Error at line '#{line}': unsupported addressing mode (#{page_parser[1]}) - only X or Y"
          end
        else
          byte = cleanup_byte_string(parameters).to_hex
          if byte <= 0xff # zero page
            return {
              :instruction => instruction,
              :addressMode => AddressingModes::ZeroPage,
              :parameters => [ byte ]
            }
          else # absolute
            return {
              :instruction => instruction,
              :addressMode => AddressingModes::Absolute,
              :parameters => high_low_part_of_byte(byte)
            }
          end
        end
      elsif parameters[0] == "(" && parameters[parameters.length - 1] == ")" # indirect or indexed indirect
        tag_content = parameters[1..parameters.length - 2]

        if tag_content.index(",") # indexed indirect
          tag_content_parser = tag_content.split(",")
          byte = cleanup_byte_string(tag_content_parser[0]).to_hex
          register = tag_content_parser[1]
          raise "Error at line '#{line}': unsupported register #{register} - only X is supported in indexed indirect addressing." if register != "X"
          raise "Error at line '#{line}': only 1 byte is allowed." if byte > 0xff

          return {
            :instruction => instruction,
            :addressMode => AddressingModes::IndexedIndirect,
            :parameters => [ byte ]
          }
        else # indirect
          byte = cleanup_byte_string(tag_content).to_hex

          return {
            :instruction => instruction,
            :addressMode => AddressingModes::Indirect,
            :parameters => (byte > 0xff ? high_low_part_of_byte(byte) : [ byte ] )
          }
        end
      elsif parameters[0] == "(" && parameters.index(",") # indirect indexed
        tag_content_parser = parameters.split(",")
        byte = cleanup_byte_string(parameters[1..parameters.length - 2]).to_hex
        register = tag_content_parser[1]
        raise "Error at line '#{line}': unsupported register #{register} - only Y is supported in indirect indexed addressing." if register != "Y"
        raise "Error at line '#{line}': only 1 byte is allowed." if byte > 0xff

        return {
          :instruction => instruction,
          :addressMode => AddressingModes::IndirectIndexed,
          :parameters => [ byte ]
        }
      end
    end
  end

  def assemble_file(file_name)
    File.open(file_name, "r") do |infile|
      while (line = infile.gets)
        line = line.strip.chomp.upcase
        parsed_line = parse_line(line)
        puts parsed_line.inspect
      end
    end

    return 0
  end

end

asm = Assembler.new
bytes = asm.assemble_file("code.asm");

puts bytes.inspect