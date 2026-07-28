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
  // s 플래그가 없으면 `.`이 개행 계열 문자(U+000A/000D/2028/2029)에 매칭되지 않아
  // "a\n.." 처럼 개행 뒤에 오는 `..`가 선행 부정 탐색을 통과한다
  @Matches(/^(?!.*\.\.)[^/\\]+$/s, { message: 'filename must not contain path separators' })
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
