#include "render_widget.h"
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QKeyEvent>

static const char* vertexShaderSource = R"(
    attribute vec2 aPosition;
    attribute vec2 aTexCoord;
    varying vec2 vTexCoord;
    void main() {
        gl_Position = vec4(aPosition, 0.0, 1.0);
        vTexCoord = aTexCoord;
    }
)";

static const char* fragmentShaderSource = R"(
    varying vec2 vTexCoord;
    uniform sampler2D uTexture;
    void main() {
        gl_FragColor = texture2D(uTexture, vTexCoord);
    }
)";

RenderWidget::RenderWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(256, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

RenderWidget::~RenderWidget() {
    makeCurrent();
    delete texture_;
    delete program_;
    doneCurrent();
}

void RenderWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Create shader program
    program_ = new QOpenGLShaderProgram(this);
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    program_->link();

    // Create texture
    createTexture();
}

void RenderWidget::createTexture() {
    if (texture_) {
        delete texture_;
    }
    texture_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture_->setSize(texWidth_, texHeight_);
    texture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
    texture_->setMinificationFilter(QOpenGLTexture::Nearest);
    texture_->setMagnificationFilter(QOpenGLTexture::Nearest);
    texture_->allocateStorage();
}

void RenderWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void RenderWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (!texture_ || !framebuffer_) return;

    // Update texture
    texture_->bind();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth_, texHeight_,
                    GL_BGRA, GL_UNSIGNED_BYTE, framebuffer_);

    // Render quad
    program_->bind();
    
    static const float vertices[] = {
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
    };

    int posLoc = program_->attributeLocation("aPosition");
    int texLoc = program_->attributeLocation("aTexCoord");

    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices + 2);

    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(texLoc);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(posLoc);
    glDisableVertexAttribArray(texLoc);

    program_->release();
    texture_->release();
}

void RenderWidget::present(const uint32_t* framebuffer) {
    framebuffer_ = framebuffer;
    update();
}

void RenderWidget::setScale(int scale) {
    scale_ = scale;
    // Don't use setFixedSize — let the widget resize with the window
    // Keep minimum size based on scale
    setMinimumSize(256 * scale_, 240 * scale_);
}

void RenderWidget::keyPressEvent(QKeyEvent* event) {
    // Forward to parent widget
    event->ignore();
}

void RenderWidget::keyReleaseEvent(QKeyEvent* event) {
    // Forward to parent widget
    event->ignore();
}
