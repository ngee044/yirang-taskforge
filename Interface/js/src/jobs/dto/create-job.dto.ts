import { Type } from 'class-transformer';
import {
  ArrayMaxSize,
  IsArray,
  IsInt,
  IsOptional,
  IsString,
  Length,
  Matches,
  Max,
  Min,
  ValidateNested,
} from 'class-validator';

// 계약(docs/API.md)의 snake_case 필드명을 그대로 사용한다.
export class InputFileDto {
  @IsString()
  @Length(1, 255)
  @Matches(/^(?!.*\.\.)[^/\\]+$/, { message: 'filename must not contain path separators' })
  filename!: string;
}

export class CreateJobDto {
  @IsString()
  @Length(1, 128)
  task_name!: string;

  @IsOptional()
  @IsArray()
  @IsString({ each: true })
  @ArrayMaxSize(64)
  arguments?: string[];

  @IsOptional()
  @IsArray()
  @ValidateNested({ each: true })
  @Type(() => InputFileDto)
  @ArrayMaxSize(32)
  input_files?: InputFileDto[];

  @IsOptional()
  @IsInt()
  @Min(1)
  @Max(3600)
  timeout_sec?: number;
}
