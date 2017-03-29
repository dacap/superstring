#include "text-buffer-wrapper.h"
#include <fstream>
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "text-wrapper.h"
#include "text-slice.h"
#include "noop.h"

using namespace v8;
using std::move;
using std::vector;

void TextBufferWrapper::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextBuffer").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop));
  prototype_template->Set(Nan::New("getLength").ToLocalChecked(), Nan::New<FunctionTemplate>(get_length));
  prototype_template->Set(Nan::New("getExtent").ToLocalChecked(), Nan::New<FunctionTemplate>(get_extent));
  prototype_template->Set(Nan::New("getTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text_in_range));
  prototype_template->Set(Nan::New("setTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text_in_range));
  prototype_template->Set(Nan::New("getText").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text));
  prototype_template->Set(Nan::New("setText").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text));
  prototype_template->Set(Nan::New("lineLengthForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_length_for_row));
  prototype_template->Set(Nan::New("lineEndingForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_ending_for_row));
  prototype_template->Set(Nan::New("characterIndexForPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(character_index_for_position));
  prototype_template->Set(Nan::New("positionForCharacterIndex").ToLocalChecked(), Nan::New<FunctionTemplate>(position_for_character_index));
  prototype_template->Set(Nan::New("load").ToLocalChecked(), Nan::New<FunctionTemplate>(load));
  prototype_template->Set(Nan::New("save").ToLocalChecked(), Nan::New<FunctionTemplate>(save));
  exports->Set(Nan::New("TextBuffer").ToLocalChecked(), constructor_template->GetFunction());
}

void TextBufferWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  TextBufferWrapper *wrapper = new TextBufferWrapper();
  wrapper->Wrap(info.This());
}

void TextBufferWrapper::get_length(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<Number>(text_buffer.size()));
}

void TextBufferWrapper::get_extent(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(PointWrapper::from_point(text_buffer.extent()));
}

void TextBufferWrapper::get_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  if (range) {
    Local<String> result;
    auto text = text_buffer.text_in_range(*range);
    if (Nan::New<String>(text.content.data(), text.content.size()).ToLocal(&result)) {
      info.GetReturnValue().Set(result);
    }
  }
}

void TextBufferWrapper::get_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  Local<String> result;
  auto text = text_buffer.text();
  if (Nan::New<String>(text.content.data(), text.content.size()).ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void TextBufferWrapper::set_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  auto text = TextWrapper::text_from_js(info[1]);
  if (range && text) {
    text_buffer.set_text_in_range(*range, move(*text));
  }
}

void TextBufferWrapper::set_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto text = TextWrapper::text_from_js(info[0]);
  if (text) {
    text_buffer.set_text(move(*text));
  }
}

void TextBufferWrapper::line_length_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto row = Nan::To<uint32_t>(info[0]);
  if (row.IsJust()) {
    info.GetReturnValue().Set(Nan::New<Number>(text_buffer.line_length_for_row(row.FromJust())));
  }
}

void TextBufferWrapper::line_ending_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto row = Nan::To<uint32_t>(info[0]);
  if (row.IsJust()) {
    info.GetReturnValue().Set(Nan::New<String>(text_buffer.line_ending_for_row(row.FromJust())).ToLocalChecked());
  }
}

void TextBufferWrapper::character_index_for_position(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto position = PointWrapper::point_from_js(info[0]);
  if (position) {
    info.GetReturnValue().Set(
      Nan::New<Number>(text_buffer.clip_position(*position).offset)
    );
  }
}

void TextBufferWrapper::position_for_character_index(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto offset = Nan::To<uint32_t>(info[0]);
  if (offset.IsJust()) {
    info.GetReturnValue().Set(
      PointWrapper::from_point(text_buffer.position_for_offset(offset.FromJust()))
    );
  }
}

class TextBufferLoader : public Nan::AsyncProgressWorkerBase<size_t> {
  Nan::Callback *progress_callback;
  TextBuffer *buffer;
  std::string file_name;
  std::string encoding_name;
  optional<Text> loaded_text;

public:
  TextBufferLoader(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
                   TextBuffer *buffer, std::string &&file_name,
                   std::string &&encoding_name) :
    AsyncProgressWorkerBase(completion_callback),
    progress_callback{progress_callback},
    buffer{buffer},
    file_name{file_name},
    encoding_name(encoding_name) {}

  void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
    static size_t CHUNK_SIZE = 10 * 1024;
    std::ifstream file{file_name};
    auto beginning = file.tellg();
    file.seekg(0, std::ios::end);
    auto end = file.tellg();
    file.seekg(0);
    loaded_text = Text::build(
      file,
      end - beginning,
      encoding_name.c_str(),
      CHUNK_SIZE,
      [&progress](size_t percent_done) { progress.Send(&percent_done, 1); }
    );
  }

  void HandleProgressCallback(const size_t *percent_done, size_t count) {
    if (!progress_callback) return;
    Nan::HandleScope scope;
    v8::Local<v8::Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(*percent_done))};
    progress_callback->Call(1, argv);
  }

  void HandleOKCallback() {
    bool result = loaded_text && buffer->reset(move(*loaded_text));
    v8::Local<v8::Value> argv[] = {Nan::New<Boolean>(result)};
    callback->Call(1, argv);
  }
};

class TextBufferSaver : public Nan::AsyncWorker {
  const TextBuffer::Snapshot *snapshot;
  std::string file_name;
  std::string encoding_name;
  bool result;

public:
  TextBufferSaver(Nan::Callback *completion_callback, const TextBuffer::Snapshot *snapshot,
                  std::string &&file_name, std::string &&encoding_name) :
    AsyncWorker(completion_callback),
    snapshot{snapshot},
    file_name{file_name},
    encoding_name(encoding_name),
    result{true} {}

  void Execute() {
    static size_t CHUNK_SIZE = 10 * 1024;
    std::ofstream file{file_name};
    for (TextSlice &chunk : snapshot->chunks()) {
      if (!Text::write(file, encoding_name.c_str(), CHUNK_SIZE, chunk)) {
        result = false;
        return;
      }
    }
  }

  void HandleOKCallback() {
    delete snapshot;
    v8::Local<v8::Value> argv[] = {Nan::New<Boolean>(result)};
    callback->Call(1, argv);
  }
};

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  std::string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  std::string encoding_name = *String::Utf8Value(info[1].As<String>());

  Nan::Callback *completion_callback = new Nan::Callback(info[2].As<Function>());

  Nan::Callback *progress_callback = nullptr;
  if (info[3]->IsFunction()) {
    progress_callback = new Nan::Callback(info[3].As<Function>());
  }

  Nan::AsyncQueueWorker(new TextBufferLoader(
    completion_callback,
    progress_callback,
    &text_buffer,
    move(file_path),
    move(encoding_name)
  ));
}

void TextBufferWrapper::save(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  std::string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  std::string encoding_name = *String::Utf8Value(info[1].As<String>());

  Nan::Callback *completion_callback = new Nan::Callback(info[2].As<Function>());
  Nan::AsyncQueueWorker(new TextBufferSaver(
    completion_callback,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name)
  ));
}
